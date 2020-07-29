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
#include "arch.h"
#include "codeCache.h"
#include "engine.h"
#include "stackFrame.h"


// Describes OpenJDK function being intercepted
class Trap {
  private:
    const char* _func_name;
    instruction_t* _entry;
    instruction_t _breakpoint_insn;
    instruction_t _saved_insn;

  public:
    Trap(const char* func_name) : _func_name(func_name), _entry(NULL), _breakpoint_insn(BREAKPOINT) {
    }

    bool resolve(NativeCodeCache* libjvm);
    void install();
    void uninstall();

    friend class AllocTracer;
};


class AllocTracer : public Engine {
  private:
    // JDK 7-9
    static Trap _in_new_tlab;
    static Trap _outside_tlab;
    // JDK 10+
    static Trap _in_new_tlab2;
    static Trap _outside_tlab2;

    static u64 _interval;
    static volatile u64 _allocated_bytes;

    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);
    static void recordAllocation(void* ucontext, StackFrame& frame, uintptr_t rklass, uintptr_t rsize, bool outside_tlab);

  public:
    const char* name() {
        return "alloc";
    }

    const char* units() {
        return "bytes";
    }

    CStack cstack() {
        return CSTACK_NO;
    }

    Error check(Arguments& args);
    Error start(Arguments& args);
    void stop();
};

#endif // _ALLOCTRACER_H
