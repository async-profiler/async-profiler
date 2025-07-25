/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _ENGINE_H
#define _ENGINE_H

#include "arguments.h"


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

    virtual Error check(Arguments& args);
    virtual Error start(Arguments& args);
    virtual void stop();

    void enableEvents(bool enabled) {
        _enabled = enabled;
    }
};

#endif // _ENGINE_H
