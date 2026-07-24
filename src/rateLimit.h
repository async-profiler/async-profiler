/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _RATELIMIT_H
#define _RATELIMIT_H

#include "arch.h"
#include "arguments.h"
#include "event.h"

#define MAP_EVENT(TYPE, CATEGORY)  ((u64)(CATEGORY ^ 15) << (TYPE * 4))
#define EVENT_TO_CATEGORY(TYPE)    ((EVENT_TO_CATEGORY_MAP >> (TYPE * 4)) & 15)

static_assert(EC_CATEGORIES < 15, "required for the above macros");
static_assert(EVENT_TYPES <= 16, "required for the above macros");

// Invert the entire map so that unmapped events resolve to the unused category
constexpr u64 EVENT_TO_CATEGORY_MAP = ~(
    MAP_EVENT(PERF_SAMPLE, EC_CPU) |
    MAP_EVENT(EXECUTION_SAMPLE, EC_CPU) |
    MAP_EVENT(WALL_CLOCK_SAMPLE, EC_WALL) |
    MAP_EVENT(NATIVE_LOCK_SAMPLE, EC_NATIVELOCK) |
    MAP_EVENT(MALLOC_SAMPLE, EC_NATIVEMEM) |
    MAP_EVENT(INSTRUMENTED_METHOD, EC_CPU) |
    MAP_EVENT(METHOD_TRACE, EC_TRACE) |
    MAP_EVENT(ALLOC_SAMPLE, EC_ALLOC) |
    MAP_EVENT(ALLOC_OUTSIDE_TLAB, EC_ALLOC) |
    MAP_EVENT(LOCK_SAMPLE, EC_LOCK) |
    MAP_EVENT(PARK_SAMPLE, EC_LOCK) |
    MAP_EVENT(SPAN, EC_SPAN));


class RateLimit {
  private:
    struct Budget {
        long budget;
        long limit;
        char padding[64 - 2 * sizeof(long)];  // padding against false-sharing
    };

    static Budget _budget[EC_CATEGORIES];
    static int _enabled_categories;

  public:
    static void enable(Arguments& args);
    static void disable();

    static void refill();

    static bool allow(EventType event_type) {
        int category = EVENT_TO_CATEGORY(event_type);
        if (_enabled_categories & (1 << category)) {
            return atomicDec(_budget[category].budget) > 0;
        }
        return true;
    }
};

#endif // _RATELIMIT_H
