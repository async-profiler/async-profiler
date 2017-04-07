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

#ifndef _PERFEVENT_H
#define _PERFEVENT_H

#include <jvmti.h>
#include <signal.h>
#include <linux/perf_event.h>
#include "spinLock.h"

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;

const size_t PAGE_SIZE = 4096;


class RingBuffer {
  private:
    const char* _start;
    u32 _offset;

  public:
    RingBuffer(struct perf_event_mmap_page* page) {
        _start = (const char*)page + PAGE_SIZE;
    }

    struct perf_event_header* seek(u64 offset) {
        _offset = (u32)(offset & 0xfff);
        return (struct perf_event_header*)(_start + _offset);
    }

    u64 next() {
        _offset = (_offset + sizeof(u64)) & 0xfff;
        return *(u64*)(_start + _offset);
    }
};

class PerfEvent : public SpinLock {
  private:
    static int _max_events;
    static PerfEvent* _events;

    int _fd;
    struct perf_event_mmap_page* _page;

    static int tid();
    static int getMaxPid();
    static void createForThread(int tid);
    static void createForAllThreads();
    static void destroyForThread(int tid);
    static void destroyForAllThreads();

  public:
    static void init();
    static void start();
    static void stop();
    static void reenable(siginfo_t* siginfo);
    static int getCallChain(const void** callchain, int max_depth);

    static void JNICALL ThreadStart(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
    static void JNICALL ThreadEnd(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread);
};

#endif // _PERFEVENT_H
