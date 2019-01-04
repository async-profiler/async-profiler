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

#if defined(__arm__) || defined(__thumb__)

#include "stackFrame.h"


uintptr_t& StackFrame::pc() {
    return (uintptr_t&)_ucontext->uc_mcontext.arm_pc;
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)_ucontext->uc_mcontext.arm_sp;
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)_ucontext->uc_mcontext.arm_fp;
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)_ucontext->uc_mcontext.arm_r0;
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)_ucontext->uc_mcontext.arm_r1;
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)_ucontext->uc_mcontext.arm_r2;
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)_ucontext->uc_mcontext.arm_r3;
}

void StackFrame::ret() {
    _ucontext->uc_mcontext.arm_pc = _ucontext->uc_mcontext.arm_lr;
}

bool StackFrame::pop(bool trust_frame_pointer) {
    return false;
}

#endif // defined(__arm__) || defined(__thumb__)
