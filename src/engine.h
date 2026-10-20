/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ENGINE_H
#define _ENGINE_H

#include <algorithm>
#include <limits.h>
#include "arguments.h"
#include "tsc.h"


class Engine {
  protected:
    static volatile bool _enabled;

    static bool updateCounter(volatile unsigned long long& counter, unsigned long long value, unsigned long long interval) {
        if (interval <= 1) {
            return true;
        }

        while (true) {
            unsigned long long prev = counter;
            unsigned long long next = prev + value;
            if (next < interval) {
                if (__sync_bool_compare_and_swap(&counter, prev, next)) {
                    return false;
                }
            } else {
                if (__sync_bool_compare_and_swap(&counter, prev, next % interval)) {
                    return true;
                }
            }
        }
    }

  public:
    virtual const char* type() {
        return "noop";
    }

    virtual const char* title() {
        return "Flame Graph";
    }

    virtual const char* units() {
        return "total";
    }

    virtual long interval() {
        return 1;
    }

    virtual Error start(Arguments& args);
    virtual void stop();

    void enableEvents(bool enabled) {
        _enabled = enabled;
    }
};

class IntervalRateLimiter {
  private:
    long _limit;
    u64 _min_interval;
    volatile u64 _window_start;
    u64 _window_bytes;

  public:
    void start(Arguments& args, int category, u64 min_interval) {
        _limit = args._output == OUTPUT_JFR ? args._rate_limit[category] : -1;
        _min_interval = min_interval;
        _window_start = TSC::ticks();
        _window_bytes = 0;
    }

    void update(volatile u64& interval, u64 bytes) {
        if (_limit <= 0) {
            return;
        }

        u64 window_start = _window_start;
        u64 window_bytes = atomicInc(_window_bytes, bytes) + bytes;
        u64 now = TSC::ticks();
        u64 elapsed = now - window_start;
        if (elapsed >= TSC::frequency()) {
            _window_start = now;
            _window_bytes = 0;
            u64 elapsed_ms = elapsed * 1000 / TSC::frequency();
            interval = std::min(std::max(window_bytes * 1000 / elapsed_ms / _limit, _min_interval), (u64)INT_MAX);
        }
    }
};

#endif // _ENGINE_H
