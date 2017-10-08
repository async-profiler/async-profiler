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


class RingBuffer {
  private:
    const char* _start;
    unsigned long _offset;

  public:
    RingBuffer(struct perf_event_mmap_page* page) {
        _start = (const char*)page + PAGE_SIZE;
    }

    struct perf_event_header* seek(u64 offset) {
        _offset = (unsigned long)offset & PAGE_MASK;
        return (struct perf_event_header*)(_start + _offset);
    }

    u64 next() {
        _offset = (_offset + sizeof(u64)) & PAGE_MASK;
        return *(u64*)(_start + _offset);
    }
};


class PerfEvent : public SpinLock {
  private:
    int _fd;
    struct perf_event_mmap_page* _page;

    friend class PerfEvents;
};


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


int PerfEvents::_max_events = 0;
PerfEvent* PerfEvents::_events = NULL;
int PerfEvents::_interval;


void PerfEvents::init() {
    _max_events = getMaxPID();
    _events = (PerfEvent*)calloc(_max_events, sizeof(PerfEvent));
}

int PerfEvents::tid() {
    return syscall(__NR_gettid);
}

void PerfEvents::createForThread(int tid) {
    struct perf_event_attr attr = {0};
    attr.type = PERF_TYPE_SOFTWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.sample_period = _interval;
    attr.sample_type = PERF_SAMPLE_CALLCHAIN;
    attr.disabled = 1;
    attr.wakeup_events = 1;
    attr.exclude_idle = 1;
    attr.precise_ip = 2;

    int fd = syscall(__NR_perf_event_open, &attr, tid, -1, -1, 0);
    if (fd == -1) {
        perror("perf_event_open failed");
        return;
    }

    void* page = mmap(NULL, 2 * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
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
        munmap(event->_page, 2 * PAGE_SIZE);
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
    Profiler::_instance.recordSample(ucontext, 1, NULL);
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_REFRESH, 1);
}

bool PerfEvents::start(int interval) {
    if (interval <= 0) return false;
    _interval = interval;

    installSignalHandler();

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, NULL);

    createForAllThreads();
    return true;
}

void PerfEvents::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_END, NULL);

    destroyForAllThreads();
}

int PerfEvents::getCallChain(const void** callchain, int max_depth) {
    PerfEvent* event = &_events[tid()];
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
