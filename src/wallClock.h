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
    enum Mode {
        CPU_ONLY,
        WALL_BATCH,
        WALL_LEGACY
    };

    static long _interval;
    static int _signal;
    static Mode _mode;

    volatile bool _running;
    pthread_t _thread;

    void timerLoop();

    static void* threadEntry(void* wall_clock) {
        ((WallClock*)wall_clock)->timerLoop();
        return NULL;
    }

    static ThreadState getThreadState(void* ucontext);

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);

    static void recordWallClock(u64 start_time, ThreadState state, u32 samples, int tid, u32 call_trace_id);

  public:
    const char* type() {
        return "wall";
    }

    const char* title() {
        return _mode == CPU_ONLY ? "CPU profile" : "Wall clock profile";
    }

    const char* units() {
        return "ns";
    }

    Error start(Arguments& args);
    void stop();
};

#endif // _WALLCLOCK_H
