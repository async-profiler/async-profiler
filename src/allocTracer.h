/*
 * Copyright 2017 Andrei Pangin
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

#ifndef _ALLOCTRACER_H
#define _ALLOCTRACER_H

#include <signal.h>
#include <stdint.h>
#include "engine.h"
#include "trap.h"


class AllocTracer : public Engine {
  private:
    static int _trap_kind;
    static Trap _in_new_tlab;
    static Trap _outside_tlab;

    static u64 _interval;
    static volatile u64 _allocated_bytes;

    static void recordAllocation(void* ucontext, int event_type, uintptr_t rklass,
                                 uintptr_t total_size, uintptr_t instance_size);

  public:
    const char* title() {
        return "Allocation profile";
    }

    const char* units() {
        return "bytes";
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();

    static void trapHandler(int signo, siginfo_t* siginfo, void* ucontext);
};

#endif // _ALLOCTRACER_H
