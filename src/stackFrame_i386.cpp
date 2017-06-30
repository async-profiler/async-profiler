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

#ifdef __i386__

#include <ucontext.h>
#include "stackFrame.h"


static inline uintptr_t* regs(void* ucontext) {
    return (uintptr_t*)((ucontext_t*)ucontext)->uc_mcontext.gregs;
}

uintptr_t& StackFrame::pc(void* ucontext) {
    return regs(ucontext)[REG_EIP];
}

uintptr_t& StackFrame::sp(void* ucontext) {
    return regs(ucontext)[REG_ESP];
}

uintptr_t& StackFrame::fp(void* ucontext) {
    return regs(ucontext)[REG_EBP];
}


static inline bool withinCurrentStack(uintptr_t value) {
    // Check that value is not too far from stack pointer of current context
    void* real_sp;
    return value - (uintptr_t)&real_sp <= 0xffff;
}

bool StackFrame::pop() {
    if (!withinCurrentStack(_sp)) {
        return false;
    }

    if (_fp == _sp || withinCurrentStack(stackAt(0))) {
        fp(_ucontext) = stackAt(0);
        pc(_ucontext) = stackAt(1);
        sp(_ucontext) = _sp + 8;
    } else {
        pc(_ucontext) = stackAt(0);
        sp(_ucontext) = _sp + 4;
    }
    return true;
}

#endif // __i386__
