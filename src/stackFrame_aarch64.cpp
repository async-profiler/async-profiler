/*
 * Copyright 2017 Andrei Pangin
 * Copyright 2017 BellSoft LLC
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
 *
 * Author: Dmitry Samersoff
 */

#if defined(__aarch64__)

#include "stackFrame.h"


#define REG_FP 29
#define REG_LR 30

uintptr_t& StackFrame::pc() {
    return (uintptr_t&)_ucontext->uc_mcontext.pc;
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)_ucontext->uc_mcontext.sp;
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)_ucontext->uc_mcontext.regs[REG_FP];
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)_ucontext->uc_mcontext.regs[0];
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)_ucontext->uc_mcontext.regs[1];
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)_ucontext->uc_mcontext.regs[2];
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)_ucontext->uc_mcontext.regs[3];
}

void StackFrame::ret() {
    _ucontext->uc_mcontext.pc = _ucontext->uc_mcontext.regs[REG_LR];
}

bool StackFrame::pop(bool trust_frame_pointer) {
    if (fp() == sp()) {
        // Expected frame layout:
        // sp   000000nnnnnnnnnn  [stack]
        // sp+8 000000nnnnnnnnnn  [link]
        fp() = stackAt(0);
        pc() = stackAt(1);
        sp() += 16;
    } else {
        // Hope LR holds correct value
        pc() = _ucontext->uc_mcontext.regs[REG_LR];
    }
    return true;
}

#endif // defined(__aarch64__)
