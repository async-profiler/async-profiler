/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _EVENT_H
#define _EVENT_H

#include <stdint.h>
#include "asprof.h"
#include "os.h"


// The order is important: look for event_type comparison
enum EventType {
    PERF_SAMPLE,
    EXECUTION_SAMPLE,
    WALL_CLOCK_SAMPLE,
    NATIVE_LOCK_SAMPLE,
    MALLOC_SAMPLE,
    INSTRUMENTED_METHOD,
    METHOD_TRACE,
    ALLOC_SAMPLE,
    ALLOC_OUTSIDE_TLAB,
    LIVE_OBJECT,
    LOCK_SAMPLE,
    PARK_SAMPLE,
    PROFILING_WINDOW,
    SPAN,
    USER_EVENT,
    EVENT_TYPES
};

class Event {
  public:
    u64 _start_time;
};

class EventWithClass : public Event {
  private:
    const char* _class_name;
    u32 _class_name_len;

  public:
    EventWithClass() : _class_name(nullptr) {}

    void setClassName(const char* class_name, u32 len);
    void setClassSignature(const char* class_sig);

    // Must be called under the sample lock (Profiler::_locks).
    // May allocate; do not call in asynchronous signal handlers.
    // TODO: revisit this if Profiler::_class_map lifecycle changes.
    u32 classId() const;
};

class ExecutionEvent : public Event {
  public:
    ThreadState _thread_state;

    ExecutionEvent(u64 start_time) {
        _start_time = start_time;
        _thread_state = THREAD_UNKNOWN;
    }
};

class MethodTraceEvent : public Event {
  public:
    u64 _duration;
};

class WallClockEvent : public Event {
  public:
    u64 _time_span;
    ThreadState _thread_state;
    u32 _samples;
};

class AllocEvent : public EventWithClass {
  public:
    u64 _total_size;
    u64 _instance_size;
};

class LockEvent : public EventWithClass {
  public:
    u64 _end_time;
    uintptr_t _address;
    long long _timeout;
};

class NativeLockEvent : public Event {
  public:
    u64 _end_time;
    uintptr_t _address;
};

class LiveObject : public EventWithClass {
  public:
    u64 _alloc_size;
    u64 _alloc_time;
};

class MallocEvent : public Event {
  public:
    uintptr_t _address;
    u64 _size;
};

class SpanEvent : public Event {
  public:
    u64 _end_time;
    const char* _tag;
};

class UserEvent : public Event {
  public:
    asprof_jfr_event_key _type;
    const uint8_t* _data;
    size_t _len;
};

#endif // _EVENT_H
