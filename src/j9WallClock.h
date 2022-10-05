/*
 * Copyright 2021 Andrei Pangin
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

#ifndef _J9WALLCLOCK_H
#define _J9WALLCLOCK_H

#include <pthread.h>
#include "engine.h"


class J9WallClock : public Engine {
  private:
    static volatile bool _enabled;
    static long _interval;

    bool _sample_idle_threads;
    int _max_stack_depth;
    volatile bool _running;
    pthread_t _thread;

    static void* threadEntry(void* wall_clock) {
        ((J9WallClock*)wall_clock)->timerLoop();
        return NULL;
    }

    void timerLoop();

  public:
    const char* title() {
        return _sample_idle_threads ? "J9 WallClock Profiler" : "J9 Execution Profiler";
    }

    const char* units() {
        return "ns";
    }

    const char* name() {
        return _sample_idle_threads ? "J9WallClock" : "J9Execution";
    }

    inline void sampleIdleThreads() {
      _sample_idle_threads = true;
    }

    Error start(Arguments& args);
    void stop();

    inline void enableEvents(bool enabled) {
      _enabled = enabled;
    }
};

#endif // _J9WALLCLOCK_H
