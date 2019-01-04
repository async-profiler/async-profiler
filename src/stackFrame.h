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

#ifndef _STACKFRAME_H
#define _STACKFRAME_H

#include <stdint.h>
#include <ucontext.h>


class StackFrame {
  private:
    ucontext_t* _ucontext;

    uintptr_t stackAt(int slot) {
        return ((uintptr_t*)sp())[slot];
    }

  public:
    StackFrame(void* ucontext) {
        _ucontext = (ucontext_t*)ucontext;
    }

    void restore(uintptr_t saved_pc, uintptr_t saved_sp, uintptr_t saved_fp) {
        pc() = saved_pc;
        sp() = saved_sp;
        fp() = saved_fp;
    }

    uintptr_t& pc();
    uintptr_t& sp();
    uintptr_t& fp();

    uintptr_t arg0();
    uintptr_t arg1();
    uintptr_t arg2();
    uintptr_t arg3();

    void ret();

    bool pop(bool trust_frame_pointer);
};

#endif // _STACKFRAME_H
