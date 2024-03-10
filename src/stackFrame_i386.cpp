/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __i386__

#include <errno.h>
#include <string.h>
#include "stackFrame.h"
#include "vmStructs.h"


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

uintptr_t StackFrame::link() {
    // No link register on x86
    return 0;
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

uintptr_t StackFrame::jarg0() {
    // Unimplemented
    return 0;
}

uintptr_t StackFrame::method() {
    return _ucontext->uc_mcontext.gregs[REG_ESP];
}

uintptr_t StackFrame::senderSP() {
    return _ucontext->uc_mcontext.gregs[REG_ESI];
}

void StackFrame::ret() {
    pc() = stackAt(0);
    sp() += 4;
}


bool StackFrame::unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    if (ip == entry || *ip == 0xc3
        || strncmp(name, "itable", 6) == 0
        || strncmp(name, "vtable", 6) == 0
        || strcmp(name, "InlineCacheBuffer") == 0)
    {
        pc = *(uintptr_t*)sp;
        sp += 4;
        return true;
    } else if (entry != NULL && entry[0] == 0x55 && entry[1] == 0x8b && entry[2] == 0xec) {
        // The stub begins with
        //   push ebp
        //   mov  ebp, esp
        if (ip == entry + 1) {
            pc = ((uintptr_t*)sp)[1];
            sp += 8;
            return true;
        } else if (withinCurrentStack(fp)) {
            sp = fp + 8;
            fp = ((uintptr_t*)sp)[-2];
            pc = ((uintptr_t*)sp)[-1];
            return true;
        }
    }
    return false;
}

bool StackFrame::unwindCompiled(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    instruction_t* entry = (instruction_t*)nm->entry();
    if (ip <= entry
        || *ip == 0xc3      // ret
        || *ip == 0x55      // push ebp
        || ip[-1] == 0x5d)  // after pop ebp
    {
        pc = *(uintptr_t*)sp;
        sp += 4;
        return true;
    } else if (*ip == 0x5d) {
        // pop ebp
        fp = ((uintptr_t*)sp)[0];
        pc = ((uintptr_t*)sp)[1];
        sp += 8;
        return true;
    }
    return false;
}

bool StackFrame::unwindAtomicStub(const void*& pc) {
    // Not needed
    return false;
}

void StackFrame::adjustSP(const void* entry, const void* pc, uintptr_t& sp) {
    // Not needed
}

bool StackFrame::skipFaultInstruction() {
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
