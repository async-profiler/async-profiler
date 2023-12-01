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

#ifndef _CPUENGINE_H
#define _CPUENGINE_H

#include <signal.h>
#include "engine.h"


// Base class for CPU sampling engines: PerfEvents, CTimer, ITimer
class CpuEngine : public Engine {
  protected:
    static void** _pthread_entry;
    static CpuEngine* _current;

    static long _interval;
    static CStack _cstack;
    static int _signal;

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void signalHandlerJ9(int signo, siginfo_t* siginfo, void* ucontext);

    static bool setupThreadHook();

    void enableThreadHook();
    void disableThreadHook();

    int createForAllThreads();

    virtual int createForThread(int tid) { return -1; }
    virtual void destroyForThread(int tid) {}

  public:
    const char* title() {
        return "CPU profile";
    }

    const char* units() {
        return "ns";
    }

    static void onThreadStart();
    static void onThreadEnd();
};

#endif // _CPUENGINE_H
