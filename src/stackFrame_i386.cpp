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

#include <errno.h>
#include <string.h>
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

uintptr_t& StackFrame::retval() {
    return (uintptr_t&)_ucontext->uc_mcontext.gregs[REG_EAX];
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

bool StackFrame::popStub(instruction_t* entry, const char* name) {
    instruction_t* ip = (instruction_t*)pc();
    if (ip == entry || *ip == 0xc3
        || strncmp(name, "itable", 6) == 0
        || strncmp(name, "vtable", 6) == 0
        || strcmp(name, "InlineCacheBuffer") == 0)
    {
        pc() = stackAt(0);
        sp() += 4;
        return true;
    } else if (entry != NULL && entry[0] == 0x55 && entry[1] == 0x8b && entry[2] == 0xec) {
        // The stub begins with
        //   push ebp
        //   mov  ebp, esp
        if (ip == entry + 1) {
            pc() = stackAt(1);
            sp() += 8;
            return true;
        } else if (withinCurrentStack(fp())) {
            sp() = fp() + 8;
            fp() = stackAt(-2);
            pc() = stackAt(-1);
            return true;
        }
    }
    return false;
}

bool StackFrame::popMethod(instruction_t* entry) {
    instruction_t* ip = (instruction_t*)pc();
    if (ip <= entry || *ip == 0xc3 || *ip == 0x55        // ret or push ebp
        || (((uintptr_t)ip & 0xfff) && ip[-1] == 0x5d))  // after pop ebp
    {
        pc() = stackAt(0);
        sp() += 4;
        return true;
    } else if (*ip == 0x5d) {
        // pop ebp
        fp() = stackAt(0);
        pc() = stackAt(1);
        sp() += 8;
        return true;
    }
    return false;
}

bool StackFrame::checkInterruptedSyscall() {
    return retval() == (uintptr_t)-EINTR;
}

bool StackFrame::isSyscall(instruction_t* pc) {
    // int 0x80
    return pc[0] == 0xcd && pc[1] == 0x80;
}

#endif // __i386__
