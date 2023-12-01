/*
 * Copyright 2023 Andrei Pangin
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

#ifndef _CTIMER_H
#define _CTIMER_H

#include "cpuEngine.h"

#ifdef __linux__

class CTimer : public CpuEngine {
  private:
    static int _max_timers;
    static int* _timers;

    int createForThread(int tid);
    void destroyForThread(int tid);

  public:
    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    static bool supported() {
        return true;
    }
};

#else

class CTimer : public CpuEngine {
  public:
    Error check(Arguments& args) {
        return Error("CTimer is not supported on this platform");
    }

    Error start(Arguments& args) {
        return Error("CTimer is not supported on this platform");
    }

    static bool supported() {
        return false;
    }
};

#endif // __linux__

#endif // _CTIMER_H
