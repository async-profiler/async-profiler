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
