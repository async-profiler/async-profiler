/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __x86_64__

#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include "stackFrame.h"
#include "vmStructs.h"


#ifdef __APPLE__
#  define REG(l, m)  _ucontext->uc_mcontext->__ss.__##m
#else
#  define REG(l, m)  _ucontext->uc_mcontext.gregs[REG_##l]
#endif


uintptr_t& StackFrame::pc() {
    return (uintptr_t&)REG(RIP, rip);
}

uintptr_t& StackFrame::sp() {
    return (uintptr_t&)REG(RSP, rsp);
}

uintptr_t& StackFrame::fp() {
    return (uintptr_t&)REG(RBP, rbp);
}

uintptr_t& StackFrame::retval() {
    return (uintptr_t&)REG(RAX, rax);
}

uintptr_t StackFrame::link() {
    // No link register on x86
    return 0;
}

uintptr_t StackFrame::arg0() {
    return (uintptr_t)REG(RDI, rdi);
}

uintptr_t StackFrame::arg1() {
    return (uintptr_t)REG(RSI, rsi);
}

uintptr_t StackFrame::arg2() {
    return (uintptr_t)REG(RDX, rdx);
}

uintptr_t StackFrame::arg3() {
    return (uintptr_t)REG(RCX, rcx);
}

uintptr_t StackFrame::jarg0() {
    return arg1();
}

uintptr_t StackFrame::method() {
    return (uintptr_t)REG(RBX, rbx);
}

uintptr_t StackFrame::senderSP() {
    return (uintptr_t)REG(R13, r13);
}

void StackFrame::ret() {
    pc() = stackAt(0);
    sp() += 8;
}


bool StackFrame::unwindStub(instruction_t* entry, const char* name, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    if (ip == entry || *ip == 0xc3
        || strncmp(name, "itable", 6) == 0
        || strncmp(name, "vtable", 6) == 0
        || strcmp(name, "InlineCacheBuffer") == 0)
    {
        pc = ((uintptr_t*)sp)[0] - 1;
        sp += 8;
        return true;
    } else if (entry != NULL && *(unsigned int*)entry == 0xec8b4855) {
        // The stub begins with
        //   push rbp
        //   mov  rbp, rsp
        if (ip == entry + 1) {
            pc = ((uintptr_t*)sp)[1] - 1;
            sp += 16;
            return true;
        } else if (withinCurrentStack(fp)) {
            sp = fp + 16;
            fp = ((uintptr_t*)sp)[-2];
            pc = ((uintptr_t*)sp)[-1] - 1;
            return true;
        }
    }
    return false;
}

bool StackFrame::unwindCompiled(NMethod* nm, uintptr_t& pc, uintptr_t& sp, uintptr_t& fp) {
    instruction_t* ip = (instruction_t*)pc;
    instruction_t* entry = (instruction_t*)nm->entry();
    if (ip <= entry
        || *ip == 0xc3                                                          // ret
        || *ip == 0x55                                                          // push rbp
        || ip[-1] == 0x5d                                                       // after pop rbp
        || (ip[0] == 0x41 && ip[1] == 0x85 && ip[2] == 0x02 && ip[3] == 0xc3))  // poll return
    {
        // Subtract 1 for PC to point to the call instruction,
        // otherwise it may be attributed to a wrong bytecode
        pc = ((uintptr_t*)sp)[0] - 1;
        sp += 8;
        return true;
    } else if (*ip == 0x5d) {
        // pop rbp
        fp = ((uintptr_t*)sp)[0];
        pc = ((uintptr_t*)sp)[1] - 1;
        sp += 16;
        return true;
    } else if (ip <= entry + 15 && ((uintptr_t)ip & 0xfff) && ip[-1] == 0x55) {
        // push rbp
        pc = ((uintptr_t*)sp)[1] - 1;
        sp += 16;
        return true;
    } else if (ip <= entry + 7 && ip[0] == 0x48 && ip[1] == 0x89 && ip[2] == 0x6c && ip[3] == 0x24) {
        // mov [rsp + #off], rbp
        sp += ip[4] + 16;
        pc = ((uintptr_t*)sp)[-1] - 1;
        return true;
    } else if ((ip[0] == 0x41 && ip[1] == 0x81 && ip[2] == 0x7f && *(u32*)(ip + 4) == 1) ||
               (ip >= entry + 8 && ip[-8] == 0x41 && ip[-7] == 0x81 && ip[-6] == 0x7f && *(u32*)(ip - 4) == 1)) {
        // cmp [r15 + #off], 1
        // nmethod_entry_barrier: frame is fully constructed here
        sp += nm->frameSize() * sizeof(void*);
        fp = ((uintptr_t*)sp)[-2];
        pc = ((uintptr_t*)sp)[-1];
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

// Skip failed MOV instruction by writing 0 to destination register
bool StackFrame::skipFaultInstruction() {
    unsigned int insn = *(unsigned int*)pc();
    if ((insn & 0x80fff8) == 0x008b48) {
        // mov r64, [r64 + offs]
        unsigned int reg = ((insn << 1) & 8) | ((insn >> 19) & 7);
        switch (reg) {
            case 0x0: REG(RAX, rax) = 0; break;
            case 0x1: REG(RCX, rcx) = 0; break;
            case 0x2: REG(RDX, rdx) = 0; break;
            case 0x3: REG(RBX, rbx) = 0; break;
            case 0x4: return false;  // Do not modify RSP
            case 0x5: REG(RBP, rbp) = 0; break;
            case 0x6: REG(RSI, rsi) = 0; break;
            case 0x7: REG(RDI, rdi) = 0; break;
            case 0x8: REG(R8 , r8 ) = 0; break;
            case 0x9: REG(R9 , r9 ) = 0; break;
            case 0xa: REG(R10, r10) = 0; break;
            case 0xb: REG(R11, r11) = 0; break;
            case 0xc: REG(R12, r12) = 0; break;
            case 0xd: REG(R13, r13) = 0; break;
            case 0xe: REG(R14, r14) = 0; break;
            case 0xf: REG(R15, r15) = 0; break;
        }

        unsigned int insn_size = 3;
        if ((insn & 0x070000) == 0x040000) insn_size++;
        if ((insn & 0x400000) == 0x400000) insn_size++;
        pc() += insn_size;
        return true;
    }
    return false;
}

bool StackFrame::checkInterruptedSyscall() {
#ifdef __APPLE__
    // We are not interested in syscalls that do not check error code, e.g. semaphore_wait_trap
    if (*(instruction_t*)pc() == 0xc3) {
        return true;
    }
    // If CF is set, the error code is in low byte of eax,
    // some other syscalls (ulock_wait) do not set CF when interrupted
    if (REG(EFL, rflags) & 1) {
        return (retval() & 0xff) == EINTR || (retval() & 0xff) == ETIMEDOUT;
    } else {
        return retval() == (uintptr_t)-EINTR;
    }
#else
    if (retval() == (uintptr_t)-EINTR) {
        // Workaround for JDK-8237858: restart the interrupted poll() manually.
        // Check if the previous instruction is mov eax, SYS_poll with infinite timeout or
        // mov eax, SYS_ppoll with any timeout (ppoll adjusts timeout automatically)
        uintptr_t pc = this->pc();
        if ((pc & 0xfff) >= 7 && *(instruction_t*)(pc - 7) == 0xb8) {
            int nr = *(int*)(pc - 6);
            if (nr == SYS_ppoll
                || (nr == SYS_poll && (int)REG(RDX, rdx) == -1)
                || (nr == SYS_epoll_wait && (int)REG(R10, r10) == -1)
                || (nr == SYS_epoll_pwait && (int)REG(R10, r10) == -1)) {
                this->pc() = pc - 7;
            }
        }
        return true;
    }
    return false;
#endif
}

bool StackFrame::isSyscall(instruction_t* pc) {
    return pc[0] == 0x0f && pc[1] == 0x05;
}

#endif // __x86_64__
