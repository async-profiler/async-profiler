/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <limits.h>
#include "rateLimit.h"
#include "tsc.h"

RateLimit::Budget RateLimit::_budget[EC_CATEGORIES];
int RateLimit::_enabled_categories = 0;

void RateLimit::enable(Arguments& args, int adaptive_categories) {
    int enabled_categories = 0;
    for (int i = 0; i < EC_CATEGORIES; i++) {
        if (args._rate_limit[i] >= 0 && !(args._rate_limit[i] > 0 && (adaptive_categories & (1 << i)))) {
            _budget[i].budget = _budget[i].limit = args._rate_limit[i];
            enabled_categories |= 1 << i;
        }
    }
    storeRelease(_enabled_categories, enabled_categories);
}

void RateLimit::disable() {
    storeRelease(_enabled_categories, 0);
}

void RateLimit::refill() {
    if (_enabled_categories == 0) return;

    for (int i = 0; i < EC_CATEGORIES; i++) {
        if (_enabled_categories & (1 << i)) {
            // Allow up to 100% budget carryover to account for short bursts
            long carryover = std::min(std::max(_budget[i].budget, 0L), _budget[i].limit);
            storeRelease(_budget[i].budget, _budget[i].limit + carryover);
        }
    }
}

void IntervalRateLimiter::start(Arguments& args, int category, u64 min_interval) {
    u64 limit = args._output == OUTPUT_JFR && args._rate_limit[category] > 0 ? args._rate_limit[category] : 0;
    if (args._alloc_window > 0) {
        _limit = limit;
        _window_ms = args._alloc_window;
    } else {
        _limit = limit * DEFAULT_RATE_WINDOW / 1000;
        _window_ms = DEFAULT_RATE_WINDOW;
    }
    _min_interval = min_interval;
    _window_start = TSC::ticks();
    _window_bytes = 0;
}

bool IntervalRateLimiter::update(u64 size, u64& interval) {
    if (_limit == 0) {
        return false;
    }

    u64 weight = size > interval ? size : interval;
    u64 window_start = _window_start;
    u64 window_bytes = atomicInc(_window_bytes, weight) + weight;
    u64 now = TSC::ticks();
    u64 elapsed_ms = (now - window_start) * 1000 / TSC::frequency();
    if (elapsed_ms >= _window_ms) {
        _window_start = now;
        _window_bytes = 0;
        u64 new_interval = std::min(std::max(window_bytes / elapsed_ms * _window_ms / _limit, _min_interval), (u64)INT_MAX);
        if (new_interval != interval) {
            interval = new_interval;
            return true;
        }
    }
    return false;
}
