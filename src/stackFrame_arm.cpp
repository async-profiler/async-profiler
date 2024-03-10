/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(__arm__) || defined(__thumb__)

#include <errno.h>
#include <string.h>
#include "stackFrame.h"
#include "vmStructs.h"


uintptr_t& StackFrame::pc() {
    return (uintptr_t&)_ucontext->uc_mcontext.arm_pc;
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)_ucontext->uc_mcontext.arm_sp;
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)_ucontext->uc_mcontext.arm_fp;
}

uintptr_t& StackFrame::retval() {
    return (uintptr_t&)_ucontext->uc_mcontext.arm_r0;
}

uintptr_t StackFrame::link() {
    return (uintptr_t)_ucontext->uc_mcontext.arm_lr;
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

uintptr_t StackFrame::jarg0() {
    // Unimplemented
    return 0;
}

uintptr_t StackFrame::method() {
    return (uintptr_t)_ucontext->uc_mcontext.arm_r9;
}

uintptr_t StackFrame::senderSP() {
    return (uintptr_t)_ucontext->uc_mcontext.arm_r4;
}

void StackFrame::ret() {
    pc() = link();
}


bool StackFrame::unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    if (ip == entry || *ip == 0xe12fff1e
        || strncmp(name, "itable", 6) == 0
        || strncmp(name, "vtable", 6) == 0
        || strcmp(name, "InlineCacheBuffer") == 0)
    {
        pc = link();
        return true;
    }
    return false;
}

bool StackFrame::unwindCompiled(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    instruction_t* entry = (instruction_t*)nm->entry();
    if (ip > entry && ip <= entry + 4 && (*ip & 0xffffff00) == 0xe24dd000) {
        //    push  {r11, lr}
        //    mov   r11, sp (optional)
        // -> sub   sp, sp, #offs
        fp = ((uintptr_t*)sp)[0];
        pc = ((uintptr_t*)sp)[1];
        sp += 8;
        return true;
    } else if (*ip == 0xe8bd4800) {
        //    add   sp, sp, #offs
        // -> pop   {r11, lr}
        fp = ((uintptr_t*)sp)[0];
        pc = ((uintptr_t*)sp)[1];
        sp += 8;
        return true;
    }
    pc = link();
    return true;
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
    // swi #0
    return *pc == 0xef000000;
}

#endif // defined(__arm__) || defined(__thumb__)
