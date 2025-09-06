/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _TSC_H
#define _TSC_H

#include "arguments.h"
#include "os.h"


const u64 NANOTIME_FREQ = 1000000000;


#if defined(__x86_64__) || defined(__i386__)

#include <cpuid.h>

#define TSC_SUPPORTED true

static inline u64 rdtsc() {
#if defined(__x86_64__)
    u32 lo, hi;
    asm volatile("rdtsc" : "=a" (lo), "=d" (hi));
    return ((u64)hi << 32) | lo;
#else
    u64 result;
    asm volatile("rdtsc" : "=A" (result));
    return result;
#endif
}

// Returns true if this CPU has a good ("invariant") timestamp counter
static bool cpuHasGoodTimestampCounter() {
    unsigned int eax, ebx, ecx, edx;

    // Check if CPUID supports misc feature flags
    __cpuid(0x80000000, eax, ebx, ecx, edx);
    if (eax < 0x80000007) {
        return 0;
    }

    // Get misc feature flags
    __cpuid(0x80000007, eax, ebx, ecx, edx);

    // Bit 8 of EDX indicates invariant TSC
    return (edx & (1 << 8)) != 0;
}

#elif defined(__aarch64__)

#define TSC_SUPPORTED true

static inline u64 rdtsc() {
    u64 value;
    asm volatile("mrs %0, cntvct_el0" : "=r"(value));
    return value;
}

static bool cpuHasGoodTimestampCounter() {
    // AARCH64 always has a good timestamp counter.
    return true;
}

#else

#define TSC_SUPPORTED false
#define rdtsc() 0

static bool cpuHasGoodTimestampCounter() {
    return false;
}

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

    // Ticks per second.
    // When using the TSC with no JVM, since there is no calibration,
    // this function will return an incorrect value.
    static u64 frequency() {
        return enabled() ? _frequency : NANOTIME_FREQ;
    }
};

#endif // _TSC_H
