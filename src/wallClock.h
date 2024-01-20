/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _WALLCLOCK_H
#define _WALLCLOCK_H

#include <jvmti.h>
#include <signal.h>
#include <pthread.h>
#include "engine.h"
#include "os.h"


class WallClock : public Engine {
  private:
    static long _interval;
    static int _signal;
    static bool _sample_idle_threads;

    volatile bool _running;
    pthread_t _thread;

    void timerLoop();

    static void* threadEntry(void* wall_clock) {
        ((WallClock*)wall_clock)->timerLoop();
        return NULL;
    }

    static ThreadState getThreadState(void* ucontext);

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);

    static long adjustInterval(long interval, int thread_count);

  public:
    const char* title() {
        return _sample_idle_threads ? "Wall clock profile" : "CPU profile";
    }

    const char* units() {
        return "ns";
    }

    Error start(Arguments& args);
    void stop();
};

#endif // _WALLCLOCK_H
