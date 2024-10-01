/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _EVENT_H
#define _EVENT_H

#include <stdint.h>
#include "os.h"


// The order is important: look for event_type comparison
enum EventType {
    PERF_SAMPLE,
    EXECUTION_SAMPLE,
    WALL_CLOCK_SAMPLE,
    INSTRUMENTED_METHOD,
    ALLOC_SAMPLE,
    ALLOC_OUTSIDE_TLAB,
    LIVE_OBJECT,
    LOCK_SAMPLE,
    PARK_SAMPLE,
    PROFILING_WINDOW,
};

class Event {
};

class EventWithClassId : public Event {
  public:
    u32 _class_id;
};

class ExecutionEvent : public Event {
  public:
    u64 _start_time;
    ThreadState _thread_state;

    ExecutionEvent(u64 start_time) : _start_time(start_time), _thread_state(THREAD_UNKNOWN) {}
};

class WallClockEvent : public Event {
  public:
    u64 _start_time;
    ThreadState _thread_state;
    u32 _samples;
};

class AllocEvent : public EventWithClassId {
  public:
    u64 _start_time;
    u64 _total_size;
    u64 _instance_size;
};

class LockEvent : public EventWithClassId {
  public:
    u64 _start_time;
    u64 _end_time;
    uintptr_t _address;
    long long _timeout;
};

class LiveObject : public EventWithClassId {
  public:
    u64 _start_time;
    u64 _alloc_size;
    u64 _alloc_time;
};

class ProfilingWindow : public Event {
  public:
    u64 _start_time;
    u64 _end_time;
};

#endif // _EVENT_H
