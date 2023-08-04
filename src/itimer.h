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

#ifndef _ITIMER_H
#define _ITIMER_H

#include <signal.h>
#include "engine.h"


class ITimer : public Engine {
  private:
    static long _interval;
    static CStack _cstack;
    static int _max_native_stack_depth;

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void signalHandlerJ9(int signo, siginfo_t* siginfo, void* ucontext);

  public:
    const char* title() {
        return "CPU profile";
    }

    const char* units() {
        return "ns";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();
};

#endif // _ITIMER_H
