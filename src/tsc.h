/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _TSC_H
#define _TSC_H

#include "arguments.h"
#include "os.h"


const u64 NANOTIME_FREQ = 1000000000;


#if defined(__x86_64__)

#define TSC_SUPPORTED true

static inline u64 rdtsc() {
    u32 lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
}

#elif defined(__i386__)

#define TSC_SUPPORTED true

static inline u64 rdtsc() {
    u64 result;
    asm volatile("rdtsc" : "=A" (result));
    return result;
}

#else

#define TSC_SUPPORTED false
#define rdtsc() 0

#endif


class TSC {
  private:
    static bool _initialized;
    static bool _available;
    static bool _enabled;
    static u64 _offset;
    static u64 _frequency;

  public:
    static void enable(Clock clock);

    static bool enabled() {
        return TSC_SUPPORTED && _enabled;
    }

    static u64 ticks() {
        return enabled() ? rdtsc() - _offset : OS::nanotime();
    }

    static u64 frequency() {
        return enabled() ? _frequency : NANOTIME_FREQ;
    }
};

#endif // _TSC_H
