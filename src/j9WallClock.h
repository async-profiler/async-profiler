/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _J9WALLCLOCK_H
#define _J9WALLCLOCK_H

#include <pthread.h>
#include "engine.h"


class J9WallClock : public Engine {
  private:
    static long _interval;

    int _max_stack_depth;
    volatile bool _running;
    pthread_t _thread;

    static void* threadEntry(void* wall_clock) {
        ((J9WallClock*)wall_clock)->timerLoop();
        return NULL;
    }

    void timerLoop();

  public:
    const char* type() {
        return "j9_wall";
    }

    const char* title() {
        return "Wall clock profile";
    }

    const char* units() {
        return "ns";
    }

    Error start(Arguments& args);
    void stop();
};

#endif // _J9WALLCLOCK_H
