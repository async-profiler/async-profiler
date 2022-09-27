/*
 * Copyright 2018 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _WALLCLOCK_H
#define _WALLCLOCK_H

#include <jvmti.h>
#include <signal.h>
#include <pthread.h>
#include "engine.h"
#include "os.h"

/**
 * This class is used for both Wall Time profiling and as the fallback for CPU Profiling (same as
 * upstream).
 *
 * Why can we simply reuse this "WallClock" engine for both Wall Time and CPU profiling?
 * That's because the "WallClock" here means it uses the wall clock to trigger the signals, as
 * opposed to using a CPU clock for example. This makes a lot of sense for Wall Time profiling, but
 * not necessarily for CPU profiling you might say. It however works well for CPU profiling as well,
 * as long as it checks whether the thread to which it is sending a signal is currently running or
 * not. That gives a statistically close enough approximation to the actual CPU time. That is also
 * the most common approach used by other profilers, JFR included.
 *
 * I did try splitting up the Wall Time and CPU profiler in different classes with some shared code
 * in a base class, however, that didn't lead to much shared code, and the main difference was the
 * classes' names.
 */
class WallClock : public Engine {
  private:
    static volatile bool _enabled;
    long _interval;
    bool _sample_idle_threads;

    // Maximum number of threads sampled in one iteration. This limit serves as a throttle
    // when generating profiling signals. Otherwise applications with too many threads may
    // suffer from a big profiling overhead. Also, keeping this limit low enough helps
    // to avoid contention on a spin lock inside Profiler::recordSample().
    long _threads_per_tick;
    bool _filtering;

    volatile bool _running;
    pthread_t _thread;

    void timerLoop();

    static void* threadEntry(void* wall_clock) {
        ((WallClock*)wall_clock)->timerLoop();
        return NULL;
    }

    static ThreadState getThreadState(void* ucontext);

    static void sharedSignalHandler(int signo, siginfo_t* siginfo, void* ucontext);
    void signalHandler(int signo, siginfo_t* siginfo, void* ucontext, u64 last_sample);

    long adjustInterval(long interval, int thread_count);

  public:
    constexpr WallClock(bool sample_idle_threads) :
        _sample_idle_threads(sample_idle_threads),
        _interval(LONG_MAX),
        _threads_per_tick(0),
        _filtering(false),
        _running(false),
        _thread(0) {}

    const char* title() {
        return _sample_idle_threads ? "Wall profile" : "CPU profile";
    }

    const char* units() {
        return "ns";
    }

    const char* name() {
        return "WallClock";
    }

    Error start(Arguments& args);
    void stop();

    inline void enableEvents(bool enabled) {
        _enabled = enabled;
    }
};

#endif // _WALLCLOCK_H
