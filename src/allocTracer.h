/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ALLOCTRACER_H
#define _ALLOCTRACER_H

#include <signal.h>
#include <stdint.h>
#include "engine.h"
#include "event.h"
#include "trap.h"


class AllocTracer : public Engine {
  private:
    static int _trap_kind;
    static Trap _in_new_tlab;
    static Trap _outside_tlab;

    static u64 _interval;
    static volatile u64 _allocated_bytes;

    static void recordAllocation(void* ucontext, EventType event_type, uintptr_t rklass,
                                 uintptr_t total_size, uintptr_t instance_size);

  public:
    const char* type() {
        return "alloc_tracer";
    }

    const char* title() {
        return "Allocation profile";
    }

    const char* units() {
        return "bytes";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    static void trapHandler(int signo, siginfo_t* siginfo, void* ucontext);
};

#endif // _ALLOCTRACER_H
