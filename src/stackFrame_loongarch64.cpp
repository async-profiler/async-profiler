/*
 * Copyright 2021 Andrei Pangin
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

#ifdef __loongarch_lp64

#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include "stackFrame.h"

#define REG(l)  _ucontext->uc_mcontext.__gregs[l]

uintptr_t& StackFrame::pc() {
    return (uintptr_t&)_ucontext->uc_mcontext.__pc;
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)REG(LARCH_REG_SP);
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)REG(22);
}

uintptr_t& StackFrame::retval() {
    return (uintptr_t&)REG(LARCH_REG_A0);
}

uintptr_t StackFrame::link() {
    return (uintptr_t)REG(LARCH_REG_RA);
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)REG(LARCH_REG_A0);
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)REG(LARCH_REG_A0 + 1);
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)REG(LARCH_REG_A0 + 2);
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)REG(LARCH_REG_A0 + 3);
}

uintptr_t StackFrame::jarg0() {
    return (uintptr_t&)REG(12);
}

uintptr_t StackFrame::method() {
    return (uintptr_t)REG(26);
}

uintptr_t StackFrame::senderSP() {
    return (uintptr_t)REG(27);
}

void StackFrame::ret() {
    pc() = link();
}

bool StackFrame::unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    if (ip == entry
        || strncmp(name, "itable", 6) == 0
        || strncmp(name, "vtable", 6) == 0
        || strcmp(name, "InlineCacheBuffer") == 0)
    {
        pc = link();
        return true;
    }
    return false;
}

bool StackFrame::unwindCompiled(instruction_t* entry, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    // Not yet implemented
    return false;
}

bool StackFrame::skipFaultInstruction() {
    return false;
}

bool StackFrame::checkInterruptedSyscall() {
    return retval() == (uintptr_t)-EINTR;
}

bool StackFrame::isSyscall(instruction_t* pc) {
    return (*pc) == 0x002b0000;
}

#endif // __loongarch_lp64
