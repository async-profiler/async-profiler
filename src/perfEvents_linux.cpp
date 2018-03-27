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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>
#include "arch.h"
#include "perfEvents.h"
#include "profiler.h"
#include "spinLock.h"


// Ancient fcntl.h does not define F_SETOWN_EX constants and structures
#ifndef F_SETOWN_EX
#define F_SETOWN_EX  15
#define F_OWNER_TID  0

struct f_owner_ex {
    int type;
    pid_t pid;
};
#endif // F_SETOWN_EX


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
static int getTracepointId(const char* name) {
    char buf[256];
    if (snprintf(buf, sizeof(buf), "/sys/kernel/debug/tracing/events/%s/id", name) >= sizeof(buf)) {
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


struct PerfEventType {
    const char* name;
    long default_interval;
    __u32 precise_ip;
    __u32 type;
    __u64 config;

    static PerfEventType AVAILABLE_EVENTS[];
    static PerfEventType KERNEL_TRACEPOINT;

    static PerfEventType* forName(const char* name) {
        // First, look through the table of predefined perf events
        for (PerfEventType* event = AVAILABLE_EVENTS; event->name != NULL; event++) {
            if (strcmp(name, event->name) == 0) {
                return event;
            }
        }

        // Second, try kernel tracepoints defined in debugfs
        if (strchr(name, ':') != NULL) {
            int tracepoint_id = getTracepointId(name);
            if (tracepoint_id > 0) {
                KERNEL_TRACEPOINT.config = tracepoint_id;
                return  &KERNEL_TRACEPOINT;
            }
        }

        return NULL;
    }
};

// See perf_event_open(2)
#define LOAD_MISS(perf_hw_cache_id) \
    ((perf_hw_cache_id) | PERF_COUNT_HW_CACHE_OP_READ << 8 | PERF_COUNT_HW_CACHE_RESULT_MISS << 16)

PerfEventType PerfEventType::AVAILABLE_EVENTS[] = {
    {"cpu",                   1000000, 2, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CPU_CLOCK},
    {"page-faults",                 1, 2, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS},
    {"context-switches",            1, 2, PERF_TYPE_SOFTWARE, PERF_COUNT_SW_CONTEXT_SWITCHES},

    {"cycles",                1000000, 2, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES},
    {"instructions",          1000000, 2, PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS},
    {"cache-references",      1000000, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_REFERENCES},
    {"cache-misses",             1000, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES},
    {"branches",              1000000, 2, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_INSTRUCTIONS},
    {"branch-misses",            1000, 2, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BRANCH_MISSES},
    {"bus-cycles",            1000000, 0, PERF_TYPE_HARDWARE, PERF_COUNT_HW_BUS_CYCLES},

    {"L1-dcache-load-misses", 1000000, 0, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_L1D)},
    {"LLC-load-misses",          1000, 0, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_LL)},
    {"dTLB-load-misses",         1000, 0, PERF_TYPE_HW_CACHE, LOAD_MISS(PERF_COUNT_HW_CACHE_DTLB)},

    {NULL}
};

PerfEventType PerfEventType::KERNEL_TRACEPOINT = {"tracepoint", 1, 0, PERF_TYPE_TRACEPOINT, 0};


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

void PerfEvents::init() {
    _max_events = getMaxPID();
    _events = (PerfEvent*)calloc(_max_events, sizeof(PerfEvent));
}

int PerfEvents::tid() {
    return syscall(__NR_gettid);
}

void PerfEvents::createForThread(int tid) {
    struct perf_event_attr attr = {0};
    attr.size = sizeof(attr);
    attr.type = _event_type->type;
    attr.config = _event_type->config;
    attr.precise_ip = _event_type->precise_ip;
    attr.sample_period = _interval;
    attr.sample_type = PERF_SAMPLE_CALLCHAIN;
    attr.disabled = 1;
    attr.wakeup_events = 1;
    attr.exclude_idle = 1;

    int fd = syscall(__NR_perf_event_open, &attr, tid, -1, -1, 0);
    if (fd == -1) {
        perror("perf_event_open failed");
        return;
    }

    void* page = mmap(NULL, 2 * PERF_PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (page == MAP_FAILED) {
        perror("perf_event mmap failed");
        page = NULL;
    }

    _events[tid].reset();
    _events[tid]._fd = fd;
    _events[tid]._page = (struct perf_event_mmap_page*)page;

    struct f_owner_ex ex;
    ex.type = F_OWNER_TID;
    ex.pid = tid;

    fcntl(fd, F_SETFL, O_ASYNC);
    fcntl(fd, F_SETSIG, SIGPROF);
    fcntl(fd, F_SETOWN_EX, &ex);

    ioctl(fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(fd, PERF_EVENT_IOC_REFRESH, 1);
}

void PerfEvents::createForAllThreads() {
    DIR* dir = opendir("/proc/self/task");
    if (dir == NULL) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] != '.') {
            int tid = atoi(entry->d_name);
            createForThread(tid);
        }
    }

    closedir(dir);
}

void PerfEvents::destroyForThread(int tid) {
    PerfEvent* event = &_events[tid];
    if (event->_fd != 0) {
        ioctl(event->_fd, PERF_EVENT_IOC_DISABLE, 0);
        close(event->_fd);
        event->_fd = 0;
    }
    if (event->_page != NULL) {
        event->lock();
        munmap(event->_page, 2 * PERF_PAGE_SIZE);
        event->_page = NULL;
        event->unlock();
    }
}

void PerfEvents::destroyForAllThreads() {
    for (int i = 0; i < _max_events; i++) {
        destroyForThread(i);
    }
}

void PerfEvents::installSignalHandler() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = NULL;
    sa.sa_sigaction = signalHandler;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;

    sigaction(SIGPROF, &sa, NULL);
}

void PerfEvents::signalHandler(int signo, siginfo_t* siginfo, void* ucontext) {
    if (siginfo->si_code <= 0) {
        // Looks like an external signal; don't treat as a profiling event
        return;
    }

    u64 counter;
    if (read(siginfo->si_fd, &counter, sizeof(counter)) != sizeof(counter)) {
        counter = 1;
    }

    Profiler::_instance.recordSample(ucontext, counter, 0, NULL);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_RESET, 0);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_REFRESH, 1);
}

Error PerfEvents::start(const char* event, long interval) {
    _event_type = PerfEventType::forName(event);
    if (_event_type == NULL) {
        return Error("Unsupported event type");
    }

    if (interval < 0) {
        return Error("interval must be positive");
    }
    _interval = interval ? interval : _event_type->default_interval;

    installSignalHandler();

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, NULL);

    createForAllThreads();
    return Error::OK;
}

void PerfEvents::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_END, NULL);

    destroyForAllThreads();
}

const char** PerfEvents::getAvailableEvents() {
    int count = sizeof(PerfEventType::AVAILABLE_EVENTS) / sizeof(PerfEventType);
    const char** available_events = new const char*[count];

    for (int i = 0; i < count; i++) {
        available_events[i] = PerfEventType::AVAILABLE_EVENTS[i].name;
    }

    return available_events;
}

int PerfEvents::getCallChain(int tid, const void** callchain, int max_depth) {
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
                    if (ip < PERF_CONTEXT_MAX && depth < max_depth) {
                        callchain[depth++] = (const void*)ip;
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

#endif // __linux__
