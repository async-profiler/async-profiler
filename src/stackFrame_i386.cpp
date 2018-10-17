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

#include "stackFrame.h"


uintptr_t& StackFrame::pc() {
    return (uintptr_t&)_ucontext->uc_mcontext.gregs[REG_EIP];
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)_ucontext->uc_mcontext.gregs[REG_ESP];
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)_ucontext->uc_mcontext.gregs[REG_EBP];
}

uintptr_t StackFrame::arg0() {
    return stackAt(1);
}

uintptr_t StackFrame::arg1() {
    return stackAt(2);
}

uintptr_t StackFrame::arg2() {
    return stackAt(3);
}

uintptr_t StackFrame::arg3() {
    return stackAt(4);
}

void StackFrame::ret() {
    pc() = stackAt(0);
    sp() += 4;
}


static inline bool withinCurrentStack(uintptr_t value) {
    // Check that value is not too far from stack pointer of current context
    void* real_sp;
    return value - (uintptr_t)&real_sp <= 0xffff;
}

bool StackFrame::pop(bool trust_frame_pointer) {
    if (!withinCurrentStack(sp())) {
        return false;
    }

    if (trust_frame_pointer && withinCurrentStack(fp())) {
        sp() = fp() + 8;
        fp() = stackAt(-2);
        pc() = stackAt(-1);
    } else if (fp() == sp() || withinCurrentStack(stackAt(0))) {
        fp() = stackAt(0);
        pc() = stackAt(1);
        sp() += 8;
    } else {
        pc() = stackAt(0);
        sp() += 4;
    }
    return true;
}

#endif // __i386__
