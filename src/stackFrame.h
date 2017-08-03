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
    uintptr_t _pc;
    uintptr_t _sp;
    uintptr_t _fp;

    uintptr_t stackAt(int slot) {
        return ((uintptr_t*)_sp)[slot];
    }

  public:
    static uintptr_t& pc(ucontext_t* ucontext);
    static uintptr_t& sp(ucontext_t* ucontext);
    static uintptr_t& fp(ucontext_t* ucontext);

    StackFrame(void* ucontext) : _ucontext((ucontext_t*)ucontext) {
        _pc = pc(_ucontext);
        _sp = sp(_ucontext);
        _fp = fp(_ucontext);
    }

    ~StackFrame() {
        pc(_ucontext) = _pc;
        sp(_ucontext) = _sp;
        fp(_ucontext) = _fp;
    }

    const void* pc() { return (const void*)_pc; }

    bool pop();
};

#endif // _STACKFRAME_H
