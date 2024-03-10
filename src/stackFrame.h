/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _STACKFRAME_H
#define _STACKFRAME_H

#include <stdint.h>
#include <ucontext.h>
#include "arch.h"


class NMethod;

class StackFrame {
  private:
    ucontext_t* _ucontext;

    static bool withinCurrentStack(uintptr_t address) {
        // Check that the address is not too far from the stack pointer of current context
        void* real_sp;
        return address - (uintptr_t)&real_sp <= 0xffff;
    }

  public:
    StackFrame(void* ucontext) {
        _ucontext = (ucontext_t*)ucontext;
    }

    void restore(uintptr_t saved_pc, uintptr_t saved_sp, uintptr_t saved_fp) {
        if (_ucontext != NULL) {
            pc() = saved_pc;
            sp() = saved_sp;
            fp() = saved_fp;
        }
    }

    uintptr_t stackAt(int slot) {
        return ((uintptr_t*)sp())[slot];
    }

    uintptr_t& pc();
    uintptr_t& sp();
    uintptr_t& fp();

    uintptr_t& retval();
    uintptr_t link();
    uintptr_t arg0();
    uintptr_t arg1();
    uintptr_t arg2();
    uintptr_t arg3();
    uintptr_t jarg0();
    uintptr_t method();
    uintptr_t senderSP();

    void ret();

    bool unwindStub(instruction_t* entry, const char* name) {
        return unwindStub(entry, name, pc(), sp(), fp());
    }

    bool unwindCompiled(NMethod* nm) {
        return unwindCompiled(nm, pc(), sp(), fp());
    }

    bool unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp);
    bool unwindCompiled(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp);
    bool unwindAtomicStub(const void*& pc);

    void adjustSP(const void* entry, const void* pc, uintptr_t& sp);

    bool skipFaultInstruction();

    bool checkInterruptedSyscall();

    // Check if PC points to a syscall instruction
    static bool isSyscall(instruction_t* pc);
};

#endif // _STACKFRAME_H
