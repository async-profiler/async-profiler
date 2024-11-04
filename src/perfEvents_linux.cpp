/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#include <jvmti.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include "arch.h"
#include "fdtransferClient.h"
#include "j9StackTraces.h"
#include "log.h"
#include "perfEvents.h"
#include "profiler.h"
#include "spinLock.h"
#include "stackFrame.h"
#include "stackWalker.h"
#include "symbols.h"
#include "tsc.h"
#include "vmStructs.h"


// Ancient fcntl.h does not define F_SETOWN_EX constants and structures
#ifndef F_SETOWN_EX
#define F_SETOWN_EX  15
#define F_OWNER_TID  0

struct f_owner_ex {
    int type;
    pid_t pid;
};
#endif // F_SETOWN_EX


enum {
    HW_BREAKPOINT_R  = 1,
    HW_BREAKPOINT_W  = 2,
    HW_BREAKPOINT_RW = 3,
    HW_BREAKPOINT_X  = 4
};


static int fetchInt(const char* file_name) {
    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    char num[16] = "0";
    ssize_t r = read(fd, num, sizeof(num) - 1);
    (void) r;
    close(fd);
    return atoi(num);
}

// Get perf_event_attr.config numeric value of the given tracepoint name
// by reading /sys/kernel/tracing/events/<name>/id (since 4.1)
// or /sys/kernel/debug/tracing/events/<name>/id (before 4.1)
static int findTracepointId(const char* dir, const char* name) {
    char buf[256];
    if ((size_t)snprintf(buf, sizeof(buf), "/sys/kernel/%s/events/%s/id", dir, name) >= sizeof(buf)) {
        return 0;
    }

    *strchr(buf, ':') = '/';  // make path from event name

    return fetchInt(buf);
}

// Get perf_event_attr.type for the given event source
// by reading /sys/bus/event_source/devices/<name>/type
static int findDeviceType(const char* name) {
    char buf[256];
    if ((size_t)snprintf(buf, sizeof(buf), "/sys/bus/event_source/devices/%s/type", name) >= sizeof(buf)) {
        return 0;
    }
    return fetchInt(buf);
}

// Convert pmu/event-name/ to pmu/param1=N,param2=M/
static void resolvePmuEventName(const char* device, char* event, size_t size) {
    char buf[256];
    if ((size_t)snprintf(buf, sizeof(buf), "/sys/bus/event_source/devices/%s/events/%s", device, event) >= sizeof(buf)) {
        return;
    }

    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return;
    }

    ssize_t r = read(fd, event, size);
    if (r > 0 && (r == size || event[r - 1] == '\n')) {
        event[r - 1] = 0;
    }
    close(fd);
}

// Set a PMU parameter (such as umask) to the corresponding config field
static bool setPmuConfig(const char* device, const char* param, __u64* config, __u64 val) {
    char buf[256];
    if ((size_t)snprintf(buf, sizeof(buf), "/sys/bus/event_source/devices/%s/format/%s", device, param) >= sizeof(buf)) {
        return false;
    }

    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return false;
    }

    ssize_t r = read(fd, buf, sizeof(buf));
    close(fd);

    if (r > 0 && r < sizeof(buf)) {
        if (strncmp(buf, "config:", 7) == 0) {
            config[0] |= val << atoi(buf + 7);
            return true;
        } else if (strncmp(buf, "config1:", 8) == 0) {
            config[1] |= val << atoi(buf + 8);
            return true;
        } else if (strncmp(buf, "config2:", 8) == 0) {
            config[2] |= val << atoi(buf + 8);
            return true;
        }
    }
    return false;
}

// Perf events consume one file descriptor per thread.
// Make sure the current limit is the highest possible.
static void adjustFDLimit() {
    struct rlimit rlim;
    if (getrlimit(RLIMIT_NOFILE, &rlim) == 0 && rlim.rlim_cur < rlim.rlim_max) {
        rlim.rlim_cur = rlim.rlim_max;
        setrlimit(RLIMIT_NOFILE, &rlim);
    }
}


struct FunctionWithCounter {
    const char* name;
    int counter_arg;
};

struct PerfEventType {
    const char* name;
    long default_interval;
    __u32 type;
    __u64 config;
    __u64 config1;
    __u64 config2;
    int counter_arg;

    enum {
        IDX_CPU = 0,
        IDX_PREDEFINED = 12,
        IDX_RAW,
        IDX_PMU,
        IDX_BREAKPOINT,
        IDX_TRACEPOINT,
        IDX_KPROBE,
        IDX_UPROBE,
    };

    static PerfEventType AVAILABLE_EVENTS[];
    static FunctionWithCounter KNOWN_FUNCTIONS[];

    static char probe_func[256];

    // Find which argument of a known function serves as a profiling counter,
    // e.g. the first argument of malloc() is allocation size
    static int findCounterArg(const char* name) {
        for (FunctionWithCounter* func = KNOWN_FUNCTIONS; func->name != NULL; func++) {
            if (strcmp(name, func->name) == 0) {
                return func->counter_arg;
            }
        }
        return 0;
    }

    // Breakpoint format: func[+offset][/len][:rwx][{arg}]
    static PerfEventType* getBreakpoint(const char* name, __u32 bp_type, __u32 bp_len) {
        char buf[256];
        strncpy(buf, name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;

        // Parse counter arg [{arg}]
        int counter_arg = 0;
        char* c = strrchr(buf, '{');
        if (c != NULL && c[1] >= '1' && c[1] <= '9') {
            *c++ = 0;
            counter_arg = atoi(c);
        }

        // Parse access type [:rwx]
        c = strrchr(buf, ':');
        if (c != NULL && c != name && c[-1] != ':') {
            *c++ = 0;
            if (strcmp(c, "r") == 0) {
                bp_type = HW_BREAKPOINT_R;
            } else if (strcmp(c, "w") == 0) {
                bp_type = HW_BREAKPOINT_W;
            } else if (strcmp(c, "x") == 0) {
                bp_type = HW_BREAKPOINT_X;
                bp_len = sizeof(long);
            } else {
                bp_type = HW_BREAKPOINT_RW;
            }
        }

        // Parse length [/8]
        c = strrchr(buf, '/');
        if (c != NULL) {
            *c++ = 0;
            bp_len = (__u32)strtol(c, NULL, 0);
        }

        // Parse offset [+0x1234]
        long long offset = 0;
        c = strrchr(buf, '+');
        if (c != NULL) {
            *c++ = 0;
            offset = strtoll(c, NULL, 0);
        }

        // Parse symbol or absolute address
        __u64 addr;
        if (strncmp(buf, "0x", 2) == 0) {
            addr = (__u64)strtoll(buf, NULL, 0);
        } else {
            addr = (__u64)(uintptr_t)dlsym(RTLD_DEFAULT, buf);
            if (addr == 0) {
                addr = (__u64)(uintptr_t)Profiler::instance()->resolveSymbol(buf);
            }
            if (c == NULL) {
                // If offset is not specified explicitly, add the default breakpoint offset
                offset = BREAKPOINT_OFFSET;
            }
        }

        if (addr == 0) {
            return NULL;
        }

        PerfEventType* breakpoint = &AVAILABLE_EVENTS[IDX_BREAKPOINT];
        breakpoint->config = bp_type;
        breakpoint->config1 = addr + offset;
        breakpoint->config2 = bp_len;
        breakpoint->counter_arg = bp_type == HW_BREAKPOINT_X && counter_arg == 0 ? findCounterArg(buf) : counter_arg;
        return breakpoint;
    }

    static PerfEventType* getTracepoint(int tracepoint_id) {
        PerfEventType* tracepoint = &AVAILABLE_EVENTS[IDX_TRACEPOINT];
        tracepoint->config = tracepoint_id;
        return tracepoint;
    }

    static PerfEventType* getProbe(PerfEventType* probe, const char* type, const char* name, __u64 ret) {
        strncpy(probe_func, name, sizeof(probe_func) - 1);
        probe_func[sizeof(probe_func) - 1] = 0;

        if (probe->type == 0 && (probe->type = findDeviceType(type)) == 0) {
            return NULL;
        }

        long long offset = 0;
        char* c = strrchr(probe_func, '+');
        if (c != NULL) {
            *c++ = 0;
            offset = strtoll(c, NULL, 0);
        }

        probe->config = ret;
        probe->config1 = (__u64)(uintptr_t)probe_func;
        probe->config2 = offset;
        return probe;
    }

    static PerfEventType* getRawEvent(__u64 config) {
        PerfEventType* raw = &AVAILABLE_EVENTS[IDX_RAW];
        raw->config = config;
        return raw;
    }

    static PerfEventType* getPmuEvent(const char* name) {
        char buf[256];
        strncpy(buf, name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;

        char* descriptor = strchr(buf, '/');
        *descriptor++ = 0;
        descriptor[strlen(descriptor) - 1] = 0;

        PerfEventType* raw = &AVAILABLE_EVENTS[IDX_PMU];
        if ((raw->type = findDeviceType(buf)) == 0) {
            return NULL;
        }

        // pmu/rNNN/
        if (descriptor[0] == 'r' && descriptor[1] >= '0') {
            char* end;
            raw->config = strtoull(descriptor + 1, &end, 16);
            if (*end == 0) {
                return raw;
            }
        }

        // Resolve event name to the list of parameters
        resolvePmuEventName(buf, descriptor, sizeof(buf) - (descriptor - buf));

        raw->config = 0;
        raw->config1 = 0;
        raw->config2 = 0;

        // Parse parameters
        while (descriptor != NULL && descriptor[0]) {
            char* p = descriptor;
            if ((descriptor = strchr(p, ',')) != NULL || (descriptor = strchr(p, ':')) != NULL) {
                *descriptor++ = 0;
            }

            __u64 val = 1;
            char* eq = strchr(p, '=');
            if (eq != NULL) {
                *eq++ = 0;
                val = strtoull(eq, NULL, 0);
            }

            if (strcmp(p, "config") == 0) {
                raw->config = val;
            } else if (strcmp(p, "config1") == 0) {
                raw->config1 = val;
            } else if (strcmp(p, "config2") == 0) {
                raw->config2 = val;
            } else if (!setPmuConfig(buf, p, &raw->config, val)) {
                return NULL;
            }
        }

        return raw;
    }

    static PerfEventType* forName(const char* name) {
        // "cpu" is an alias for "cpu-clock"
        if (strcmp(name, EVENT_CPU) == 0) {
            return &AVAILABLE_EVENTS[IDX_CPU];
        }

        // Look through the table of predefined perf events
        for (int i = 0; i <= IDX_PREDEFINED; i++) {
            if (strcmp(name, AVAILABLE_EVENTS[i].name) == 0) {
                return &AVAILABLE_EVENTS[i];
            }
        }

        // Hardware breakpoint
        if (strncmp(name, "mem:", 4) == 0) {
            return getBreakpoint(name + 4, HW_BREAKPOINT_RW, 1);
        }

        // Raw tracepoint ID
        if (strncmp(name, "trace:", 6) == 0) {
            int tracepoint_id = atoi(name + 6);
            return tracepoint_id > 0 ? getTracepoint(tracepoint_id) : NULL;
        }

        // kprobe or uprobe
        if (strncmp(name, "kprobe:", 7) == 0) {
            return getProbe(&AVAILABLE_EVENTS[IDX_KPROBE], "kprobe", name + 7, 0);
        }
        if (strncmp(name, "uprobe:", 7) == 0) {
            return getProbe(&AVAILABLE_EVENTS[IDX_UPROBE], "uprobe", name + 7, 0);
        }
        if (strncmp(name, "kretprobe:", 10) == 0) {
            return getProbe(&AVAILABLE_EVENTS[IDX_KPROBE], "kprobe", name + 10, 1);
        }
        if (strncmp(name, "uretprobe:", 10) == 0) {
            return getProbe(&AVAILABLE_EVENTS[IDX_UPROBE], "uprobe", name + 10, 1);
        }

        // Raw PMU register: rNNN
        if (name[0] == 'r' && name[1] >= '0') {
            char* end;
            __u64 reg = strtoull(name + 1, &end, 16);
            if (*end == 0) {
                return getRawEvent(reg);
            }
        }

        // Raw perf event descriptor: pmu/event-descriptor/
        const char* s = strchr(name, '/');
        if (s > name && s[1] != 0 && s[strlen(s) - 1] == '/') {
            return getPmuEvent(name);
        }

        // Kernel tracepoints defined in debugfs
        s = strchr(name, ':');
        if (s != NULL && s[1] != ':') {
            int tracepoint_id;
            if ((tracepoint_id = findTracepointId("tracing", name)) > 0 ||
                (tracepoint_id = findTracepointId("debug/tracing", name)) > 0) {
                return getTracepoint(tracepoint_id);
            }
        }

        // Finally, treat event as a function name and return an execution breakpoint
        return getBreakpoint(name, HW_BREAKPOINT_X, sizeof(long));
    }
};

// See perf_event_open(2)
#define LOAD_MISS(perf_hw_cache_id) \
    ((perf_hw_cache_id) | PERF_COUNT_HW_CACHE_OP_READ << 8 | PERF_COUNT_HW_CACHE_RESULT_MISS << 16)

// Hardware breakpoint with interval=1 causes an infinite loop on ARM64
#ifdef __aarch64__
#  define BKPT_INTERVAL 2
#else
#  define BKPT_INTERVAL 1
#endif

PerfEventType PerfEventType::AVAILABLE_EVENTS[] = {
    {"cpu-clock",    DEFAULT_INTERVAL, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK},
    {"page-faults",                 1, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS},
    {"context-switches",            2, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES},

    {"cycles",                1000000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES},
    {"instructions",          1000000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS},
    {"cache-references",      1000000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES},
    {"cache-misses",             1000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES},
    {"branch-instructions",   1000000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS},
    {"branch-misses",            1000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES},
    {"bus-cycles",            1000000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES},

    {"L1-dcache-load-misses", 1000000, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_L1D)},
    {"LLC-load-misses",          1000, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_LL)},
    {"dTLB-load-misses",         1000, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_DTLB)},

    /* End of IDX_PREDEFINED events */

    {"rNNN",                     1000, PERF_TYPE_RAW, 0}, /* IDX_RAW */
    {"pmu/event-descriptor/",    1000, PERF_TYPE_RAW, 0}, /* IDX_PMU */

    {"mem:breakpoint",  BKPT_INTERVAL, PERF_TYPE_BREAKPOINT, 0}, /* IDX_BREAKPOINT */
    {"trace:tracepoint",            1, PERF_TYPE_TRACEPOINT, 0}, /* IDX_TRACEPOINT */

    {"kprobe:func",                 1, 0, 0}, /* IDX_KPROBE */
    {"uprobe:path",                 1, 0, 0}, /* IDX_UPROBE */
};

FunctionWithCounter PerfEventType::KNOWN_FUNCTIONS[] = {
    {"malloc",   1},
    {"mmap",     2},
    {"munmap",   2},
    {"read",     3},
    {"write",    3},
    {"send",     3},
    {"recv",     3},
    {"sendto",   3},
    {"recvfrom", 3},
    {NULL}
};

char PerfEventType::probe_func[256];


class RingBuffer {
  private:
    const char* _start;
    unsigned long _offset;

  public:
    RingBuffer(struct perf_event_mmap_page* page) {
        _start = (const char*)page + OS::page_size;
    }

    struct perf_event_header* seek(u64 offset) {
        _offset = (unsigned long)offset & OS::page_mask;
        return (struct perf_event_header*)(_start + _offset);
    }

    u64 next() {
        _offset = (_offset + sizeof(u64)) & OS::page_mask;
        return *(u64*)(_start + _offset);
    }

    u64 peek(unsigned long words) {
        unsigned long peek_offset = (_offset + words * sizeof(u64)) & OS::page_mask;
        return *(u64*)(_start + peek_offset);
    }
};


class PerfEvent : public SpinLock {
  private:
    int _fd;
    struct perf_event_mmap_page* _page;

    friend class PerfEvents;
};


int PerfEvents::_max_events = 0;
PerfEvent* PerfEvents::_events = NULL;
PerfEventType* PerfEvents::_event_type = NULL;
bool PerfEvents::_alluser;
bool PerfEvents::_kernel_stack;

int PerfEvents::createForThread(int tid) {
    if (tid >= _max_events) {
        Log::warn("tid[%d] > pid_max[%d]. Restart profiler after changing pid_max", tid, _max_events);
        return -1;
    }

    // Mark _events[tid] early to prevent duplicates. Real fd will be put later.
    if (!__sync_bool_compare_and_swap(&_events[tid]._fd, 0, -1)) {
        // Lost race. The event is created either from PerfEvents::start() or from pthread hook.
        return -1;
    }

    PerfEventType* event_type = _event_type;
    struct perf_event_attr attr = {0};
    attr.size = sizeof(attr);
    attr.type = event_type->type;

    if (attr.type == PERF_TYPE_BREAKPOINT) {
        attr.bp_type = event_type->config;
    } else {
        attr.config = event_type->config;
    }
    attr.config1 = event_type->config1;
    attr.config2 = event_type->config2;

    // Hardware events may not always support zero skid
    if (attr.type == PERF_TYPE_SOFTWARE) {
        attr.precise_ip = 2;
    }

    attr.sample_period = _interval;
    attr.sample_type = PERF_SAMPLE_CALLCHAIN;
    attr.disabled = 1;
    attr.wakeup_events = 1;

    if (_alluser) {
        attr.exclude_kernel = 1;
    }

    if (!_kernel_stack) {
        attr.exclude_callchain_kernel = 1;
    }

    if (_cstack >= CSTACK_FP) {
        attr.exclude_callchain_user = 1;
    }

#ifdef PERF_ATTR_SIZE_VER5
    if (_cstack == CSTACK_LBR) {
        attr.sample_type |= PERF_SAMPLE_BRANCH_STACK | PERF_SAMPLE_REGS_USER;
        attr.branch_sample_type = PERF_SAMPLE_BRANCH_USER | PERF_SAMPLE_BRANCH_CALL_STACK;
        attr.sample_regs_user = 1ULL << PERF_REG_PC;
    }
#else
#warning "Compiling without LBR support. Kernel headers 4.1+ required"
#endif

    int fd;
    if (FdTransferClient::hasPeer()) {
        fd = FdTransferClient::requestPerfFd(&tid, &attr);
    } else {
        fd = syscall(__NR_perf_event_open, &attr, tid, -1, -1, 0);
    }

    if (fd == -1) {
        int err = errno;
        Log::warn("perf_event_open for TID %d failed: %s", tid, strerror(err));
        _events[tid]._fd = 0;
        if (isResourceLimit(err) && _current != NULL) {
            // Emergency shutdown
            stop();
        }
        return err;
    }

    void* page = NULL;
    if (_kernel_stack || _cstack == CSTACK_DEFAULT || _cstack == CSTACK_LBR) {
        page = mmap(NULL, 2 * OS::page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (page == MAP_FAILED) {
            Log::warn("perf_event mmap failed: %s", strerror(errno));
            page = NULL;
        }
    }

    _events[tid].reset();
    _events[tid]._fd = fd;
    _events[tid]._page = (struct perf_event_mmap_page*)page;

    struct f_owner_ex ex;
    ex.type = F_OWNER_TID;
    ex.pid = tid;

    int err;
    if (fcntl(fd, F_SETFL, O_ASYNC) < 0 || fcntl(fd, F_SETSIG, _signal) < 0 || fcntl(fd, F_SETOWN_EX, &ex) < 0) {
        err = errno;
        Log::warn("perf_event fcntl failed: %s", strerror(err));
    } else if (ioctl(fd, PERF_EVENT_IOC_RESET, 0) < 0 || ioctl(fd, PERF_EVENT_IOC_REFRESH, 1) < 0) {
        err = errno;
        Log::warn("perf_event ioctl failed: %s", strerror(err));
    } else {
        return 0;
    }

    // Failed to setup perf_event - rollback changes
    if (page != NULL) {
        munmap(page, 2 * OS::page_size);
        _events[tid]._page = NULL;
    }
    close(fd);
    _events[tid]._fd = 0;

    return err;
}

void PerfEvents::destroyForThread(int tid) {
    if (tid >= _max_events) {
        return;
    }

    PerfEvent* event = &_events[tid];
    int fd = event->_fd;
    if (fd > 0 && __sync_bool_compare_and_swap(&event->_fd, fd, 0)) {
        ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
        close(fd);
    }
    if (event->_page != NULL) {
        event->lock();
        munmap(event->_page, 2 * OS::page_size);
        event->_page = NULL;
        event->unlock();
    }
}

u64 PerfEvents::readCounter(siginfo_t* siginfo, void* ucontext) {
    switch (_event_type->counter_arg) {
        case 1: return StackFrame(ucontext).arg0();
        case 2: return StackFrame(ucontext).arg1();
        case 3: return StackFrame(ucontext).arg2();
        case 4: return StackFrame(ucontext).arg3();
        default: {
            u64 counter;
            return read(siginfo->si_fd, &counter, sizeof(counter)) == sizeof(counter) ? counter : 1;
        }
    }
}

void PerfEvents::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (siginfo->si_code <= 0) {
        // Looks like an external signal; don't treat as a profiling event
        return;
    }

    if (_enabled) {
        ExecutionEvent event(TSC::ticks());
        u64 counter = readCounter(siginfo, ucontext);
        Profiler::instance()->recordSample(ucontext, counter, PERF_SAMPLE, &event);
    } else {
        resetBuffer(OS::threadId());
    }

    ioctl(siginfo->si_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_REFRESH, 1);
}

void PerfEvents::signalHandlerJ9(int signo, siginfo_t* siginfo, void* ucontext) {
    if (siginfo->si_code <= 0) {
        // Looks like an external signal; don't treat as a profiling event
        return;
    }

    if (_enabled) {
        u64 counter = readCounter(siginfo, ucontext);
        J9StackTraceNotification notif;
        StackContext java_ctx;
        notif.num_frames = _cstack == CSTACK_NO ? 0 : walk(OS::threadId(), ucontext, notif.addr, MAX_J9_NATIVE_FRAMES, &java_ctx);
        J9StackTraces::checkpoint(counter, &notif);
    } else {
        resetBuffer(OS::threadId());
    }

    ioctl(siginfo->si_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_REFRESH, 1);
}

const char* PerfEvents::title() {
    if (_event_type == NULL || strcmp(_event_type->name, "cpu-clock") == 0) {
        return "CPU profile";
    } else if (_event_type->type == PERF_TYPE_SOFTWARE || _event_type->type == PERF_TYPE_HARDWARE || _event_type->type == PERF_TYPE_HW_CACHE) {
        return _event_type->name;
    } else {
        return "Flame Graph";
    }
}

const char* PerfEvents::units() {
    return _event_type == NULL || strcmp(_event_type->name, "cpu-clock") == 0 ? "ns" : "total";
}

Error PerfEvents::check(Arguments& args) {
    PerfEventType* event_type = PerfEventType::forName(args._event);
    if (event_type == NULL) {
        return Error("Unsupported event type");
    } else if (event_type->counter_arg > 4) {
        return Error("Only arguments 1-4 can be counted");
    }

    if (!setupThreadHook()) {
        return Error("Could not set pthread hook");
    }

    struct perf_event_attr attr = {0};
    attr.size = sizeof(attr);
    attr.type = event_type->type;

    if (attr.type == PERF_TYPE_BREAKPOINT) {
        attr.bp_type = event_type->config;
    } else {
        attr.config = event_type->config;
    }
    attr.config1 = event_type->config1;
    attr.config2 = event_type->config2;

    attr.sample_period = event_type->default_interval;
    attr.sample_type = PERF_SAMPLE_CALLCHAIN;
    attr.disabled = 1;

    if (args._alluser) {
        attr.exclude_kernel = 1;
    }

#ifdef PERF_ATTR_SIZE_VER5
    if (args._cstack == CSTACK_LBR) {
        attr.sample_type |= PERF_SAMPLE_BRANCH_STACK | PERF_SAMPLE_REGS_USER;
        attr.branch_sample_type = PERF_SAMPLE_BRANCH_USER | PERF_SAMPLE_BRANCH_CALL_STACK;
        attr.sample_regs_user = 1ULL << PERF_REG_PC;
    }
#endif

    int fd = syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
    if (fd == -1) {
        return Error(strerror(errno));
    }

    close(fd);
    return Error::OK;
}

Error PerfEvents::start(Arguments& args) {
    _event_type = PerfEventType::forName(args._event);
    if (_event_type == NULL) {
        return Error("Unsupported event type");
    } else if (_event_type->counter_arg > 4) {
        return Error("Only arguments 1-4 can be counted");
    }

    if (!setupThreadHook()) {
        return Error("Could not set pthread hook");
    }

    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : _event_type->default_interval;
    _cstack = args._cstack;
    _signal = args._signal == 0 ? OS::getProfilingSignal(0) : args._signal & 0xff;

    _alluser = args._alluser;
    _kernel_stack = !_alluser && _cstack != CSTACK_NO;
    if (_kernel_stack && !Symbols::haveKernelSymbols()) {
        Log::warn("Kernel symbols are unavailable due to restrictions. Try\n"
                  "  sysctl kernel.perf_event_paranoid=1\n"
                  "  sysctl kernel.kptr_restrict=0");
        _kernel_stack = false;
    }

    adjustFDLimit();

    int max_events = OS::getMaxThreadId();
    if (max_events != _max_events) {
        free(_events);
        _events = (PerfEvent*)calloc(max_events, sizeof(PerfEvent));
        _max_events = max_events;
    }

    if (VM::isOpenJ9()) {
        if (_cstack == CSTACK_DEFAULT) _cstack = CSTACK_DWARF;
        OS::installSignalHandler(_signal, signalHandlerJ9);
        Error error = J9StackTraces::start(args);
        if (error) {
            return error;
        }
    } else {
        OS::installSignalHandler(_signal, signalHandler);
    }

    // Enable pthread hook before traversing currently running threads
    enableThreadHook();

    // Create perf_events for all existing threads
    int err = createForAllThreads();
    if (err) {
        stop();
        if (err == EACCES || err == EPERM) {
            return Error("Perf events unavailable. Try --fdtransfer or --all-user option or 'sysctl kernel.perf_event_paranoid=1'");
        } else if (isResourceLimit(err)) {
            return Error("Perf events resource limit. Check 'ulimit -n'");
        } else {
            return Error("Perf events unavailable");
        }
    }
    return Error::OK;
}

void PerfEvents::stop() {
    disableThreadHook();
    for (int i = 0; i < _max_events; i++) {
        destroyForThread(i);
    }
    J9StackTraces::stop();
}

int PerfEvents::walk(int tid, void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx) {
    PerfEvent* event = &_events[tid];
    if (!event->tryLock()) {
        return 0;  // the event is being destroyed
    }

    int depth = 0;

    struct perf_event_mmap_page* page = event->_page;
    if (page != NULL) {
        u64 tail = page->data_tail;
        u64 head = page->data_head;
        rmb();

        RingBuffer ring(page);

        while (tail < head) {
            struct perf_event_header* hdr = ring.seek(tail);
            if (hdr->type == PERF_RECORD_SAMPLE) {
                u64 nr = ring.next();
                while (nr-- > 0) {
                    u64 ip = ring.next();
                    if (ip < PERF_CONTEXT_MAX) {
                        const void* iptr = (const void*)ip;
                        if (CodeHeap::contains(iptr) || depth >= max_depth) {
                            // Stop at the first Java frame
                            java_ctx->pc = iptr;
                            goto stack_complete;
                        }
                        callchain[depth++] = iptr;
                    }
                }

                if (_cstack == CSTACK_LBR) {
                    u64 bnr = ring.next();

                    // Last userspace PC is stored right after branch stack
                    const void* pc = (const void*)ring.peek(bnr * 3 + 2);
                    if (CodeHeap::contains(pc) || depth >= max_depth) {
                        java_ctx->pc = pc;
                        goto stack_complete;
                    }
                    callchain[depth++] = pc;

                    while (bnr-- > 0) {
                        const void* from = (const void*)ring.next();
                        const void* to = (const void*)ring.next();
                        ring.next();

                        if (CodeHeap::contains(to) || depth >= max_depth) {
                            java_ctx->pc = to;
                            goto stack_complete;
                        }
                        callchain[depth++] = to;

                        if (CodeHeap::contains(from) || depth >= max_depth) {
                            java_ctx->pc = from;
                            goto stack_complete;
                        }
                        callchain[depth++] = from;
                    }
                }

                break;
            }
            tail += hdr->size;
        }

stack_complete:
        page->data_tail = head;
    }

    event->unlock();

    if (_cstack == CSTACK_FP) {
        depth += StackWalker::walkFP(ucontext, callchain + depth, max_depth - depth, java_ctx);
    } else if (_cstack == CSTACK_DWARF) {
        depth += StackWalker::walkDwarf(ucontext, callchain + depth, max_depth - depth, java_ctx);
    }

    return depth;
}

void PerfEvents::resetBuffer(int tid) {
    PerfEvent* event = &_events[tid];
    if (!event->tryLock()) {
        return;  // the event is being destroyed
    }

    struct perf_event_mmap_page* page = event->_page;
    if (page != NULL) {
        u64 head = page->data_head;
        rmb();
        page->data_tail = head;
    }

    event->unlock();
}

bool PerfEvents::supported() {
    struct perf_event_attr attr = {0};
    attr.size = sizeof(attr);
    attr.type = PERF_TYPE_SOFTWARE;
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.sample_period = 1000000000;
    attr.sample_type = PERF_SAMPLE_CALLCHAIN;
    attr.disabled = 1;

    int fd = syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0);
    if (fd == -1) {
        return false;
    }

    close(fd);
    return true;
}

const char* PerfEvents::getEventName(int event_id) {
    if (event_id >= 0 && (size_t)event_id < sizeof(PerfEventType::AVAILABLE_EVENTS) / sizeof(PerfEventType)) {
        return PerfEventType::AVAILABLE_EVENTS[event_id].name;
    }
    return NULL;
}

#endif // __linux__
