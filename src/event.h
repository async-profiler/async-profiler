/*
 * Copyright 2020 Andrei Pangin
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

#ifndef _EVENT_H
#define _EVENT_H

#include <stdint.h>
#include "os.h"


// The order is important: look for event_type comparison
enum EventType {
    PERF_SAMPLE,
    EXECUTION_SAMPLE,
    INSTRUMENTED_METHOD,
    ALLOC_SAMPLE,
    ALLOC_OUTSIDE_TLAB,
    LIVE_OBJECT,
    LOCK_SAMPLE,
    PARK_SAMPLE,
    PROFILING_WINDOW,
};

class Event {
  public:
    u32 id() {
        return *(u32*)this;
    }
};

class ExecutionEvent : public Event {
  public:
    ThreadState _thread_state;

    ExecutionEvent() : _thread_state(THREAD_UNKNOWN) {
    }
};

class AllocEvent : public Event {
  public:
    u32 _class_id;
    u64 _total_size;
    u64 _instance_size;
};

class LockEvent : public Event {
  public:
    u32 _class_id;
    u64 _start_time;
    u64 _end_time;
    uintptr_t _address;
    long long _timeout;
};

class LiveObject : public Event {
  public:
    u32 _class_id;
    u64 _alloc_size;
    u64 _alloc_time;
};

class ProfilingWindow : public Event {
  public:
    u64 _start_time;
    u64 _end_time;
};

#endif // _EVENT_H
