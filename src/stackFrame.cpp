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

#include <ucontext.h>
#include "stackFrame.h"

#if defined(__arm__) || defined(__thimb__)
uintptr_t& StackFrame::pc(void* ucontext) {
    return (uintptr_t&)((ucontext_t*)ucontext)->uc_mcontext.arm_pc;
}

uintptr_t& StackFrame::sp(void* ucontext) {
    return (uintptr_t&)((ucontext_t*)ucontext)->uc_mcontext.arm_sp;
}

uintptr_t& StackFrame::fp(void* ucontext) {
    return (uintptr_t&)((ucontext_t*)ucontext)->uc_mcontext.arm_fp;
}
#else
static inline uintptr_t* regs(void* ucontext) {
    return (uintptr_t*)((ucontext_t*)ucontext)->uc_mcontext.gregs;
}

uintptr_t& StackFrame::pc(void* ucontext) {
    return regs(ucontext)[REG_RIP];
}

uintptr_t& StackFrame::sp(void* ucontext) {
    return regs(ucontext)[REG_RSP];
}

uintptr_t& StackFrame::fp(void* ucontext) {
    return regs(ucontext)[REG_RBP];
}
#endif

static inline bool withinCurrentStack(uintptr_t value) {
    // Check that value is not too far from stack pointer of current context
    void* real_sp;
    return value - (uintptr_t)&real_sp <= 0xffff;
}

static inline bool isFramePrologueEpilogue(uintptr_t pc) {
    unsigned int opcode = *(unsigned int*)(pc - 1);
    if (opcode == 0xec834855) {
        // push rbp
        // sub  rsp, $const
        return true;
    } else if (opcode == 0xec8b4855) {
        // push rbp
        // mov  rbp, rsp
        return true;
    } else if ((opcode & 0xffffff00) == 0x05855d00) {
        // pop  rbp
        // test [polling_page], eax
        return true;
    }
    return false;
}

bool StackFrame::pop() {
#if defined(__arm__) || defined(__thumb__)
    return false;
#endif

    if (!withinCurrentStack(_sp)) {
        return false;
    }

    if (_fp == _sp || withinCurrentStack(stackAt(0)) || isFramePrologueEpilogue(_pc)) {
        fp(_ucontext) = stackAt(0);
        pc(_ucontext) = stackAt(1);
        sp(_ucontext) = _sp + 16;
    } else {
        pc(_ucontext) = stackAt(0);
        sp(_ucontext) = _sp + 8;
    }
    return true;
}
