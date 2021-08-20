/*
 * Copyright 2017 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include "arch.h"
#include "log.h"
#include "os.h"
#include "perfEvents.h"
#include "profiler.h"
#include "spinLock.h"
#include "stackFrame.h"
#include "symbols.h"


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
// by reading /sys/kernel/debug/tracing/events/<name>/id file
static int findTracepointId(const char* name) {
    char buf[256];
    if ((size_t)snprintf(buf, sizeof(buf), "/sys/kernel/debug/tracing/events/%s/id", name) >= sizeof(buf)) {
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
        // Look through the table of predefined perf events
        for (int i = 0; i < IDX_PREDEFINED; i++) {
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
            int tracepoint_id = findTracepointId(name);
            if (tracepoint_id > 0) {
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

PerfEventType PerfEventType::AVAILABLE_EVENTS[] = {
    {"cpu",          DEFAULT_INTERVAL, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK},
    {"page-faults",                 1, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS},
    {"context-switches",            1, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES},

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

    {"rNNN",                     1000, PERF_TYPE_RAW, 0}, /* IDX_RAW */
    {"pmu/event-descriptor/",    1000, PERF_TYPE_RAW, 0}, /* IDX_PMU */

    {"mem:breakpoint",              1, PERF_TYPE_BREAKPOINT, 0}, /* IDX_BREAKPOINT */
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
long PerfEvents::_interval;
Ring PerfEvents::_ring;
CStack PerfEvents::_cstack;

int PerfEvents::createForThread(int tid) {
    if (tid >= _max_events) {
        Log::warn("tid[%d] > pid_max[%d]. Restart profiler after changing pid_max", tid, _max_events);
        return -1;
    }

    PerfEventType* event_type = _event_type;
    if (event_type == NULL) {
        return -1;
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

    // Hardware events may not always support zero skid
    if (attr.type == PERF_TYPE_SOFTWARE) {
        attr.precise_ip = 2;
    }

    attr.sample_period = _interval;
    attr.sample_type = PERF_SAMPLE_CALLCHAIN;
    attr.disabled = 1;
    attr.wakeup_events = 1;

    if (_ring == RING_USER) {
        attr.exclude_kernel = 1;
    } else if (_ring == RING_KERNEL) {
        attr.exclude_user = 1;
    }

#ifdef PERF_ATTR_SIZE_VER5
    if (_cstack == CSTACK_LBR) {
        attr.sample_type |= PERF_SAMPLE_BRANCH_STACK | PERF_SAMPLE_REGS_USER;
        attr.branch_sample_type = PERF_SAMPLE_BRANCH_USER | PERF_SAMPLE_BRANCH_CALL_STACK;
        attr.sample_regs_user = 1ULL << PERF_REG_PC;
        attr.exclude_callchain_user = 1;
    }
#else
#warning "Compiling without LBR support. Kernel headers 4.1+ required"
#endif

    int fd = syscall(__NR_perf_event_open, &attr, tid, -1, -1, 0);
    if (fd == -1) {
        int err = errno;
        Log::warn("perf_event_open failed: %s", strerror(errno));
        return err;
    }

    if (!__sync_bool_compare_and_swap(&_events[tid]._fd, 0, fd)) {
        // Lost race. The event is created either from start() or from onThreadStart()
        close(fd);
        return -1;
    }

    void* page = mmap(NULL, 2 * OS::page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (page == MAP_FAILED) {
        Log::warn("perf_event mmap failed: %s", strerror(errno));
        page = NULL;
    }

    _events[tid].reset();
    _events[tid]._page = (struct perf_event_mmap_page*)page;

    struct f_owner_ex ex;
    ex.type = F_OWNER_TID;
    ex.pid = tid;

    fcntl(fd, F_SETFL, O_ASYNC);
    fcntl(fd, F_SETSIG, SIGPROF);
    fcntl(fd, F_SETOWN_EX, &ex);

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);

    return 0;
}

void PerfEvents::destroyForThread(int tid) {
    if (tid >= _max_events) {
        return;
    }

    PerfEvent* event = &_events[tid];
    int fd = event->_fd;
    if (fd != 0 && __sync_bool_compare_and_swap(&event->_fd, fd, 0)) {
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

void PerfEvents::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (siginfo->si_code <= 0) {
        // Looks like an external signal; don't treat as a profiling event
        return;
    }

    if (_enabled) {
        u64 counter;
        switch (_event_type->counter_arg) {
            case 1: counter = StackFrame(ucontext).arg0(); break;
            case 2: counter = StackFrame(ucontext).arg1(); break;
            case 3: counter = StackFrame(ucontext).arg2(); break;
            case 4: counter = StackFrame(ucontext).arg3(); break;
            default:
                if (read(siginfo->si_fd, &counter, sizeof(counter)) != sizeof(counter)) {
                    counter = 1;
                }
        }

        ExecutionEvent event;
        Profiler::instance()->recordSample(ucontext, counter, 0, &event);
    } else {
        resetBuffer(OS::threadId());
    }

    ioctl(siginfo->si_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_REFRESH, 1);
}

const char* PerfEvents::title() {
    if (_event_type == NULL || _event_type->name == EVENT_CPU) {
        return "CPU profile";
    } else if (_event_type->type == PERF_TYPE_SOFTWARE || _event_type->type == PERF_TYPE_HARDWARE || _event_type->type == PERF_TYPE_HW_CACHE) {
        return _event_type->name;
    } else {
        return "Flame Graph";
    }
}

const char* PerfEvents::units() {
    return _event_type == NULL || _event_type->name == EVENT_CPU ? "ns" : "total";
}

Error PerfEvents::check(Arguments& args) {
    PerfEventType* event_type = PerfEventType::forName(args._event);
    if (event_type == NULL) {
        return Error("Unsupported event type");
    } else if (event_type->counter_arg > 4) {
        return Error("Only arguments 1-4 can be counted");
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

    if (args._ring == RING_USER) {
        attr.exclude_kernel = 1;
    } else if (args._ring == RING_KERNEL) {
        attr.exclude_user = 1;
    } else if (!Symbols::haveKernelSymbols()) {
        Profiler::instance()->updateSymbols(true);
        attr.exclude_kernel = Symbols::haveKernelSymbols() ? 0 : 1;
    }

#ifdef PERF_ATTR_SIZE_VER5
    if (args._cstack == CSTACK_LBR) {
        attr.sample_type |= PERF_SAMPLE_BRANCH_STACK | PERF_SAMPLE_REGS_USER;
        attr.branch_sample_type = PERF_SAMPLE_BRANCH_USER | PERF_SAMPLE_BRANCH_CALL_STACK;
        attr.sample_regs_user = 1ULL << PERF_REG_PC;
        attr.exclude_callchain_user = 1;
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

    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : _event_type->default_interval;

    _ring = args._ring;
    if (_ring != RING_USER && !Symbols::haveKernelSymbols()) {
        Log::warn("Kernel symbols are unavailable due to restrictions. Try\n"
                  "  sysctl kernel.kptr_restrict=0\n"
                  "  sysctl kernel.perf_event_paranoid=1");
        _ring = RING_USER;
    }
    _cstack = args._cstack;

    int max_events = OS::getMaxThreadId();
    if (max_events != _max_events) {
        free(_events);
        _events = (PerfEvent*)calloc(max_events, sizeof(PerfEvent));
        _max_events = max_events;
    }

    OS::installSignalHandler(SIGPROF, signalHandler);

    // Enable thread events before traversing currently running threads
    Profiler::instance()->switchThreadEvents(JVMTI_ENABLE);

    // Create perf_events for all existing threads
    int err;
    bool created = false;
    ThreadList* thread_list = OS::listThreads();
    for (int tid; (tid = thread_list->next()) != -1; ) {
        if ((err = createForThread(tid)) == 0) {
            created = true;
        }
    }
    delete thread_list;

    if (!created) {
        Profiler::instance()->switchThreadEvents(JVMTI_DISABLE);
        if (err == EACCES || err == EPERM) {
            return Error("No access to perf events. Try --all-user option or 'sysctl kernel.perf_event_paranoid=1'");
        } else {
            return Error("Perf events unavailable");
        }
    }
    return Error::OK;
}

void PerfEvents::stop() {
    for (int i = 0; i < _max_events; i++) {
        destroyForThread(i);
    }
}

int PerfEvents::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth,
                               CodeCache* java_methods, CodeCache* runtime_stubs) {
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
                        if (java_methods->contains(iptr) || runtime_stubs->contains(iptr) || depth >= max_depth) {
                            // Stop at the first Java frame
                            goto stack_complete;
                        }
                        callchain[depth++] = iptr;
                    }
                }

                if (_cstack == CSTACK_LBR) {
                    u64 bnr = ring.next();

                    // Last userspace PC is stored right after branch stack
                    const void* pc = (const void*)ring.peek(bnr * 3 + 2);
                    if (java_methods->contains(pc) || runtime_stubs->contains(pc) || depth >= max_depth) {
                        goto stack_complete;
                    }
                    callchain[depth++] = pc;

                    while (bnr-- > 0) {
                        const void* from = (const void*)ring.next();
                        const void* to = (const void*)ring.next();
                        ring.next();

                        if (java_methods->contains(to) || runtime_stubs->contains(to) || depth >= max_depth) {
                            goto stack_complete;
                        }
                        callchain[depth++] = to;

                        if (java_methods->contains(from) || runtime_stubs->contains(from) || depth >= max_depth) {
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
    // The official way of knowing if perf_event_open() support is enabled
    // is checking for the existence of the file /proc/sys/kernel/perf_event_paranoid
    struct stat statbuf;
    return stat("/proc/sys/kernel/perf_event_paranoid", &statbuf) == 0;
}

const char* PerfEvents::getEventName(int event_id) {
    if (event_id >= 0 && (size_t)event_id < sizeof(PerfEventType::AVAILABLE_EVENTS) / sizeof(PerfEventType)) {
        return PerfEventType::AVAILABLE_EVENTS[event_id].name;
    }
    return NULL;
}

#endif // __linux__
