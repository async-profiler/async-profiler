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

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include "perfEvent.h"
#include "vmEntry.h"

// from aarch32_port
#elif defined (__arm__) || defined(__thumb__)
#define rmb() __asm__ __volatile__ ("dmb ish"   : : : "memory")
#else
#define rmb()  asm volatile("lfence":::"memory")
#endif

// Ancient fcntl.h does not define F_SETOWN_EX constants and structures
#ifndef F_SETOWN_EX
#define F_SETOWN_EX  15
#define F_OWNER_TID  0

struct f_owner_ex {
    int type;
    pid_t pid;
};
#endif // F_SETOWN_EX


int PerfEvent::_max_events = 0;
PerfEvent* PerfEvent::_events = NULL;
int PerfEvent::_interval_cycles;


void PerfEvent::init() {
    _max_events = getMaxPid();
    _events = (PerfEvent*)calloc(_max_events, sizeof(PerfEvent));
}

int PerfEvent::tid() {
    return syscall(__NR_gettid);
}

int PerfEvent::getMaxPid() {
    char buf[16] = "65536";
    int fd = open("/proc/sys/kernel/pid_max", O_RDONLY);
    if (fd != -1) {
        ssize_t r = read(fd, buf, sizeof(buf) - 1);
        (void) r;
        close(fd);
    }
    return atoi(buf);
}

void PerfEvent::createForThread(int tid) {
    struct perf_event_attr attr = {0};
    attr.type = PERF_TYPE_SOFTWARE;
    attr.size = sizeof(attr);
    attr.config = PERF_COUNT_SW_CPU_CLOCK;
    attr.sample_period = _interval_cycles;
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
        perror("mmap failed");
        close(fd);
        return;
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

void PerfEvent::createForAllThreads() {
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

void PerfEvent::destroyForThread(int tid) {
    PerfEvent* event = &_events[tid];
    if (event->_fd != 0) {
        ioctl(event->_fd, PERF_EVENT_IOC_DISABLE, 0);
        close(event->_fd);
        event->_fd = 0;
    }
    if (event->_page != NULL) {
        event->spinLock();
        munmap(event->_page, 2 * PAGE_SIZE);
        event->_page = NULL;
        event->unlock();
    }
}

void PerfEvent::destroyForAllThreads() {
    for (int i = 0; i < _max_events; i++) {
        destroyForThread(i);
    }
}

void PerfEvent::start(int interval_cycles) {
    _interval_cycles = interval_cycles;

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_THREAD_END, NULL);

    createForAllThreads();
}

void PerfEvent::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_START, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_THREAD_END, NULL);

    destroyForAllThreads();
}

void PerfEvent::reenable(siginfo_t* siginfo) {
    ioctl(siginfo->si_fd, PERF_EVENT_IOC_REFRESH, 1);
}

int PerfEvent::getCallChain(const void** callchain, int max_depth) {
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

void JNICALL PerfEvent::ThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    createForThread(tid());
}

void JNICALL PerfEvent::ThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    destroyForThread(tid());
}
