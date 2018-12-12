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

#ifdef __x86_64__

#include "stackFrame.h"


#ifdef __APPLE__
#  define REG(l, m)  _ucontext->uc_mcontext->__ss.m
#else
#  define REG(l, m)  _ucontext->uc_mcontext.gregs[l]
#endif


uintptr_t& StackFrame::pc() {
    return (uintptr_t&)REG(REG_RIP, __rip);
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)REG(REG_RSP, __rsp);
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)REG(REG_RBP, __rbp);
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)REG(REG_RDI, __rdi);
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)REG(REG_RSI, __rsi);
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)REG(REG_RDX, __rdx);
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)REG(REG_RCX, __rcx);
}

void StackFrame::ret() {
    pc() = stackAt(0);
    sp() += 8;
}


static inline bool withinCurrentStack(uintptr_t value) {
    // Check that value is not too far from stack pointer of current context
    void* real_sp;
    return value - (uintptr_t)&real_sp <= 0xffff;
}

static inline bool isFramePrologueEpilogue(uintptr_t pc) {
    if (pc & 0xfff) {
        // Make sure we are not at the page boundary, so that reading [pc - 1] is safe
        unsigned int opcode = *(unsigned int*)(pc - 1);
        if (opcode == 0xec834855) {
            // push rbp
            // sub  rsp, $const
            return true;
        } else if (opcode == 0xec8b4855) {
            // push rbp
            // mov  rbp, rsp
            return true;
        }
    }

    if (*(unsigned char*)pc == 0x5d && *(unsigned short*)(pc + 1) == 0x0585) {
        // pop  rbp
        // test [polling_page], eax
        return true;
    }

    return false;
}

bool StackFrame::pop(bool trust_frame_pointer) {
    if (!withinCurrentStack(sp())) {
        return false;
    }

    if (trust_frame_pointer && withinCurrentStack(fp())) {
        sp() = fp() + 16;
        fp() = stackAt(-2);
        pc() = stackAt(-1);
    } else if (fp() == sp() || withinCurrentStack(stackAt(0)) || isFramePrologueEpilogue(pc())) {
        fp() = stackAt(0);
        pc() = stackAt(1);
        sp() += 16;
    } else {
        pc() = stackAt(0);
        sp() += 8;
    }
    return true;
}

#endif // __x86_64__
