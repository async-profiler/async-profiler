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
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include "arch.h"
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


static const unsigned long PERF_PAGE_SIZE = sysconf(_SC_PAGESIZE);

static int getMaxPID() {
    char buf[16] = "65536";
    int fd = open("/proc/sys/kernel/pid_max", O_RDONLY);
    if (fd != -1) {
        ssize_t r = read(fd, buf, sizeof(buf) - 1);
        (void) r;
        close(fd);
    }
    return atoi(buf);
}

// Get perf_event_attr.config numeric value of the given tracepoint name
// by reading /sys/kernel/debug/tracing/events/<name>/id file
static int findTracepointId(const char* name) {
    char buf[256];
    if ((size_t)snprintf(buf, sizeof(buf), "/sys/kernel/debug/tracing/events/%s/id", name) >= sizeof(buf)) {
        return 0;
    }

    *strchr(buf, ':') = '/';  // make path from event name

    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    char id[16] = "0";
    ssize_t r = read(fd, id, sizeof(id) - 1);
    (void) r;
    close(fd);
    return atoi(id);
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
    __u32 bp_type;
    __u32 bp_len;
    int counter_arg;

    static PerfEventType AVAILABLE_EVENTS[];
    static FunctionWithCounter KNOWN_FUNCTIONS[];

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

    static PerfEventType* findByType(__u32 type) {
        for (PerfEventType* event = AVAILABLE_EVENTS; ; event++) {
            if (event->type == type) {
                return event;
            }
        }
    }

    // Breakpoint format: func[+offset][/len][:rwx]
    static PerfEventType* getBreakpoint(const char* name, __u32 bp_type, __u32 bp_len) {
        char buf[256];
        strncpy(buf, name, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;

        // Parse access type [:rwx]
        char* c = strrchr(buf, ':');
        if (c != NULL) {
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
                addr = (__u64)(uintptr_t)Profiler::_instance.findSymbol(buf);
            }
            if (addr == 0) {
                return NULL;
            }
        }

        PerfEventType* breakpoint = findByType(PERF_TYPE_BREAKPOINT);
        breakpoint->config = addr + offset;
        breakpoint->bp_type = bp_type;
        breakpoint->bp_len = bp_len;
        breakpoint->counter_arg = bp_type == HW_BREAKPOINT_X ? findCounterArg(buf) : 0;
        return breakpoint;
    }

    static PerfEventType* getTracepoint(int tracepoint_id) {
        PerfEventType* tracepoint = findByType(PERF_TYPE_TRACEPOINT);
        tracepoint->config = tracepoint_id;
        return tracepoint;
    }

    static PerfEventType* forName(const char* name) {
        // Hardware breakpoint
        if (strncmp(name, "mem:", 4) == 0) {
            return getBreakpoint(name + 4, HW_BREAKPOINT_RW, 1);
        }

        // Raw tracepoint ID
        if (strncmp(name, "trace:", 6) == 0) {
            int tracepoint_id = atoi(name + 6);
            return tracepoint_id > 0 ? getTracepoint(tracepoint_id) : NULL;
        }

        // Look through the table of predefined perf events
        for (PerfEventType* event = AVAILABLE_EVENTS; event->name != NULL; event++) {
            if (strcmp(name, event->name) == 0) {
                return event;
            }
        }

        // Kernel tracepoints defined in debugfs
        if (strchr(name, ':') != NULL) {
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
    {"branches",              1000000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS},
    {"branch-misses",            1000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES},
    {"bus-cycles",            1000000, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES},

    {"L1-dcache-load-misses", 1000000, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_L1D)},
    {"LLC-load-misses",          1000, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_LL)},
    {"dTLB-load-misses",         1000, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_DTLB)},

    {"mem:breakpoint",              1, PERF_TYPE_BREAKPOINT, 0},
    {"trace:tracepoint",            1, PERF_TYPE_TRACEPOINT, 0},

    {NULL}
};

FunctionWithCounter PerfEventType::KNOWN_FUNCTIONS[] = {
    {"malloc",   1},
    {"mmap",     2},
    {"read",     3},
    {"write",    3},
    {"send",     3},
    {"recv",     3},
    {"sendto",   3},
    {"recvfrom", 3},
    {NULL}
};


class RingBuffer {
  private:
    const char* _start;
    unsigned long _offset;

  public:
    RingBuffer(struct perf_event_mmap_page* page) {
        _start = (const char*)page + PERF_PAGE_SIZE;
    }

    struct perf_event_header* seek(u64 offset) {
        _offset = (unsigned long)offset & (PERF_PAGE_SIZE - 1);
        return (struct perf_event_header*)(_start + _offset);
    }

    u64 next() {
        _offset = (_offset + sizeof(u64)) & (PERF_PAGE_SIZE - 1);
        return *(u64*)(_start + _offset);
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
bool PerfEvents::_print_extended_warning;

bool PerfEvents::createForThread(int tid) {
    if (tid >= _max_events) {
        fprintf(stderr, "WARNING: tid[%d] > pid_max[%d]. Restart profiler after changing pid_max\n", tid, _max_events);
        return false;
    }

    PerfEventType* event_type = _event_type;
    if (event_type == NULL) {
        return false;
    }

    struct perf_event_attr attr = {0};
    attr.size = sizeof(attr);
    attr.type = event_type->type;

    if (attr.type == PERF_TYPE_BREAKPOINT) {
        attr.bp_addr = event_type->config;
        attr.bp_type = event_type->bp_type;
        attr.bp_len = event_type->bp_len;
    } else {
        attr.config = event_type->config;
    }

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

    int fd = syscall(__NR_perf_event_open, &attr, tid, -1, -1, 0);
    if (fd == -1) {
        int err = errno;
        perror("perf_event_open failed");
        if (err == EACCES && _print_extended_warning) {
            fprintf(stderr, "Due to permission restrictions, you cannot collect kernel events.\n"
                            "Try with --all-user option, or 'echo 1 > /proc/sys/kernel/perf_event_paranoid'\n");
            _print_extended_warning = false;
        }
        return false;
    }

    if (!__sync_bool_compare_and_swap(&_events[tid]._fd, 0, fd)) {
        // Lost race. The event is created either from start() or from onThreadStart()
        close(fd);
        return false;
    }

    void* page = mmap(NULL, 2 * PERF_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (page == MAP_FAILED) {
        perror("perf_event mmap failed");
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

    return true;
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
        munmap(event->_page, 2 * PERF_PAGE_SIZE);
        event->_page = NULL;
        event->unlock();
    }
}

void PerfEvents::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (siginfo->si_code <= 0) {
        // Looks like an external signal; don't treat as a profiling event
        return;
    }

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

    Profiler::_instance.recordSample(ucontext, counter, 0, NULL);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_REFRESH, 1);
}

const char* PerfEvents::units() {
    if (_event_type == NULL || _event_type->name == EVENT_CPU) {
        return "ns";
    } else if (_event_type->type == PERF_TYPE_BREAKPOINT || _event_type->type == PERF_TYPE_TRACEPOINT) {
        return "events";
    }

    const char* dash = strrchr(_event_type->name, '-');
    return dash != NULL ? dash + 1 : _event_type->name;
}

Error PerfEvents::start(Arguments& args) {
    _event_type = PerfEventType::forName(args._event);
    if (_event_type == NULL) {
        return Error("Unsupported event type");
    }

    if (args._interval < 0) {
        return Error("interval must be positive");
    }
    _interval = args._interval ? args._interval : _event_type->default_interval;

    _ring = args._ring;
    if (_ring != RING_USER && !Symbols::haveKernelSymbols()) {
        fprintf(stderr, "WARNING: Kernel symbols are unavailable due to restrictions. Try\n"
                        "  echo 0 > /proc/sys/kernel/kptr_restrict\n"
                        "  echo 1 > /proc/sys/kernel/perf_event_paranoid\n");
        _ring = RING_USER;
    }
    _print_extended_warning = _ring != RING_USER;

    int max_events = getMaxPID();
    if (max_events != _max_events) {
        free(_events);
        _events = (PerfEvent*)calloc(max_events, sizeof(PerfEvent));
        _max_events = max_events;
    }
    
    OS::installSignalHandler(SIGPROF, signalHandler);

    // Enable thread events before traversing currently running threads
    Profiler::_instance.switchThreadEvents(JVMTI_ENABLE);

    // Create perf_events for all existing threads
    bool created = false;
    ThreadList* thread_list = OS::listThreads();
    for (int tid; (tid = thread_list->next()) != -1; ) {
        created |= createForThread(tid);
    }
    delete thread_list;

    if (!created) {
        Profiler::_instance.switchThreadEvents(JVMTI_DISABLE);
        return Error("Perf events unavailable. See stderr of the target process.");
    }
    return Error::OK;
}

void PerfEvents::stop() {
    for (int i = 0; i < _max_events; i++) {
        destroyForThread(i);
    }
}

void PerfEvents::onThreadStart() {
    createForThread(OS::threadId());
}

void PerfEvents::onThreadEnd() {
    destroyForThread(OS::threadId());
}

int PerfEvents::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth,
                               VmCodeCache* cc) {
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
                while (nr-- > 0 && depth < max_depth) {
                    u64 ip = ring.next();
                    if (ip < PERF_CONTEXT_MAX) {
                        const void* iptr = (const void*)ip;
                        callchain[depth++] = iptr;
                        if (cc->contains(iptr)) {
                            // Stop at the first Java frame
                            break;
                        }
                    }
                }
                break;
            }
            tail += hdr->size;
        }

        page->data_tail = head;
    }

    event->unlock();
    return depth;
}

bool PerfEvents::supported() {
    return true;
}

const char* PerfEvents::getEventName(int event_id) {
    if (event_id >= 0 && (size_t)event_id < sizeof(PerfEventType::AVAILABLE_EVENTS) / sizeof(PerfEventType)) {
        return PerfEventType::AVAILABLE_EVENTS[event_id].name;
    }
    return NULL;
}

#endif // __linux__
