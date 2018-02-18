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
#include "arch.h"
#include "engine.h"


// Describes OpenJDK function being intercepted
class Trap {
  private:
    const char* _func_name;
    instruction_t* _entry;
    instruction_t _saved_insn;

  public:
    Trap(const char* func_name) : _func_name(func_name), _entry(NULL) {
    }

    void install();
    void uninstall();

    friend class AllocTracer;
};


class AllocTracer : public Engine {
  private:
    static Trap _in_new_tlab;
    static Trap _outside_tlab;
    static bool _klass_is_oop;

    static uintptr_t resolveKlassHandle(uintptr_t klass_handle) {
        if (_klass_is_oop) {
            // On JDK 7 KlassHandle is a pointer to klassOop, hence one more indirection
            return *(uintptr_t*)klass_handle + 2 * sizeof(uintptr_t);
        }
        return klass_handle;
    }

    static void installSignalHandler();
    static void signalHandler(int signo, siginfo_t* siginfo, void* ucontext);

  public:
    const char* name() {
        return "alloc";
    }

    Error start(const char* event, long interval);
    void stop();
};

#endif // _ALLOCTRACER_H
