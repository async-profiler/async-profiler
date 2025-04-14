/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <setjmp.h>
#include "stackWalker.h"
#include "dwarf.h"
#include "profiler.h"
#include "safeAccess.h"
#include "stackFrame.h"
#include "vmStructs.h"


const uintptr_t SAME_STACK_DISTANCE = 8192;
const uintptr_t MAX_WALK_SIZE = 0x100000;
const intptr_t MAX_FRAME_SIZE = 0x40000;
const intptr_t MAX_INTERPRETER_FRAME_SIZE = 0x1000;
const intptr_t DEAD_ZONE = 0x1000;


static inline bool aligned(uintptr_t ptr) {
    return (ptr & (sizeof(uintptr_t) - 1)) == 0;
}

static inline bool inDeadZone(const void* ptr) {
    return ptr < (const void*)DEAD_ZONE || ptr > (const void*)-DEAD_ZONE;
}

static inline bool sameStack(void* hi, void* lo) {
    return (uintptr_t)hi - (uintptr_t)lo < SAME_STACK_DISTANCE;
}

// AArch64: on Linux, frame link is stored at the top of the frame,
// while on macOS, frame link is at the bottom.
static inline uintptr_t defaultSenderSP(uintptr_t sp, uintptr_t fp) {
#ifdef __APPLE__
    return sp + 2 * sizeof(void*);
#else
    return fp;
#endif
}

static inline void fillFrame(ASGCT_CallFrame& frame, ASGCT_CallFrameType type, const char* name) {
    frame.bci = type;
    frame.method_id = (jmethodID)name;
}

static inline void fillFrame(ASGCT_CallFrame& frame, FrameTypeId type, int bci, jmethodID method) {
    frame.bci = FrameType::encode(type, bci);
    frame.method_id = method;
}

static jmethodID getMethodId(VMMethod* method) {
    if (!inDeadZone(method) && aligned((uintptr_t)method)) {
        jmethodID method_id = method->id();
        if (!inDeadZone(method_id) && aligned((uintptr_t)method_id) && VMMethod::fromMethodID(method_id) == method) {
            return method_id;
        }
    }
    return NULL;
}


int StackWalker::walkFP(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx, bool* truncated) {
    const void* pc;
    uintptr_t fp;
    uintptr_t sp;
    uintptr_t bottom = (uintptr_t)&sp + MAX_WALK_SIZE;

    StackFrame frame(ucontext);
    if (ucontext == NULL) {
        pc = callerPC();
        fp = (uintptr_t)callerFP();
        sp = (uintptr_t)callerSP();
    } else {
        pc = (const void*)frame.pc();
        fp = frame.fp();
        sp = frame.sp();
    }

    int depth = 0;

    // Walk until the bottom of the stack or until the first Java frame
    while (true) {
        if (depth == max_depth) {
            *truncated = true;
            break;
        }
        if (CodeHeap::contains(pc) && !(depth == 0 && frame.unwindAtomicStub(pc))) {
            *truncated = true;
            java_ctx->set(pc, sp, fp);
            break;
        }

        callchain[depth++] = pc;

        // Check if the next frame is below on the current stack
        if (fp < sp || fp >= sp + MAX_FRAME_SIZE || fp >= bottom) {
            *truncated = fp != 0x0;
            break;
        }

        // Frame pointer must be word aligned
        if (!aligned(fp)) {
            *truncated = true;
            break;
        }

        pc = stripPointer(SafeAccess::load((void**)fp + FRAME_PC_SLOT));
        if (inDeadZone(pc)) {
            *truncated = pc != NULL;
            break;
        }

        sp = fp + (FRAME_PC_SLOT + 1) * sizeof(void*);
        fp = *(uintptr_t*)fp;
    }

    return depth;
}

int StackWalker::walkDwarf(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx, bool* truncated) {
    const void* pc;
    uintptr_t fp;
    uintptr_t sp;
    uintptr_t bottom = (uintptr_t)&sp + MAX_WALK_SIZE;

    StackFrame frame(ucontext);
    if (ucontext == NULL) {
        pc = callerPC();
        fp = (uintptr_t)callerFP();
        sp = (uintptr_t)callerSP();
    } else {
        pc = (const void*)frame.pc();
        fp = frame.fp();
        sp = frame.sp();
    }

    int depth = 0;
    Profiler* profiler = Profiler::instance();

    // Walk until the bottom of the stack or until the first Java frame
    while (true) {
        if (depth == max_depth) {
            *truncated = true;
            break;
        }
        if (CodeHeap::contains(pc) && !(depth == 0 && frame.unwindAtomicStub(pc))) {
            // Don't dereference pc as it may point to unreadable memory
            // frame.adjustSP(page_start, pc, sp);
            *truncated = true;
            java_ctx->set(pc, sp, fp);
            break;
        }

        callchain[depth++] = pc;

        uintptr_t prev_sp = sp;
        CodeCache* cc = profiler->findLibraryByAddress(pc);
        FrameDesc* f = cc != NULL ? cc->findFrameDesc(pc) : &FrameDesc::default_frame;

        u8 cfa_reg = (u8)f->cfa;
        int cfa_off = f->cfa >> 8;
        if (cfa_reg == DW_REG_SP) {
            sp = sp + cfa_off;
        } else if (cfa_reg == DW_REG_FP) {
            sp = fp + cfa_off;
        } else if (cfa_reg == DW_REG_PLT) {
            sp += ((uintptr_t)pc & 15) >= 11 ? cfa_off * 2 : cfa_off;
        } else {
            *truncated = true;
            break;
        }

        // Check if the next frame is below on the current stack
        if (sp < prev_sp || sp >= prev_sp + MAX_FRAME_SIZE || sp >= bottom) {
            *truncated = sp != 0x0;
            break;
        }

        // Stack pointer must be word aligned
        if (!aligned(sp)) {
            *truncated = true;
            break;
        }

        if (f->fp_off & DW_PC_OFFSET) {
            pc = (const char*)pc + (f->fp_off >> 1);
        } else {
            if (f->fp_off != DW_SAME_FP && f->fp_off < MAX_FRAME_SIZE && f->fp_off > -MAX_FRAME_SIZE) {
                fp = (uintptr_t)SafeAccess::load((void**)(sp + f->fp_off));
            }
            if (EMPTY_FRAME_SIZE > 0 || cfa_off != 0) {
                // x86 or AArch64 non-default frame
                pc = stripPointer(SafeAccess::load((void**)(sp + f->pc_off)));
            } else if (f->fp_off != DW_SAME_FP) {
                // AArch64 default_frame
                pc = stripPointer(SafeAccess::load((void**)(sp + f->pc_off)));
                sp = defaultSenderSP(sp, fp);
                if (sp < prev_sp || sp >= bottom || !aligned(sp)) {
                    *truncated = true;
                    break;
                }
            } else if (depth <= 1) {
                pc = (const void*)frame.link();
            } else {
                // Stack bottom
                break;
            }
        }

        if (inDeadZone(pc)) {
            *truncated = pc != NULL;
            break;
        }
    }

    return depth;
}

int StackWalker::walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth, StackDetail detail, bool* truncated) {
    if (ucontext == NULL) {
        return walkVM(ucontext, frames, max_depth, detail,
                      callerPC(), (uintptr_t)callerSP(), (uintptr_t)callerFP(), truncated);
    } else {
        StackFrame frame(ucontext);
        return walkVM(ucontext, frames, max_depth, detail,
                      (const void*)frame.pc(), frame.sp(), frame.fp(), truncated);
    }
}

int StackWalker::walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth, JavaFrameAnchor* anchor, bool* truncated) {
    uintptr_t sp = anchor->lastJavaSP();
    if (sp == 0) {
        return 0;
    }

    uintptr_t fp = anchor->lastJavaFP();
    if (fp == 0) {
        fp = sp;
    }

    const void* pc = anchor->lastJavaPC();
    if (pc == NULL) {
        pc = ((const void**)sp)[-1];
    }

    return walkVM(ucontext, frames, max_depth, VM_BASIC, pc, sp, fp, truncated);
}

int StackWalker::walkVM(void* ucontext, ASGCT_CallFrame* frames, int max_depth,
                        StackDetail detail, const void* pc, uintptr_t sp, uintptr_t fp, bool* truncated) {
    StackFrame frame(ucontext);
    uintptr_t bottom = (uintptr_t)&frame + MAX_WALK_SIZE;

    Profiler* profiler = Profiler::instance();
    int bcp_offset = InterpreterFrame::bcp_offset();

    jmp_buf crash_protection_ctx;
    VMThread* vm_thread = VMThread::current();
    void* saved_exception = vm_thread != NULL ? vm_thread->exception() : NULL;

    // Should be preserved across setjmp/longjmp
    volatile int depth = 0;

    if (vm_thread != NULL) {
        vm_thread->exception() = &crash_protection_ctx;
        if (setjmp(crash_protection_ctx) != 0) {
            vm_thread->exception() = saved_exception;
            if (depth < max_depth) {
                fillFrame(frames[depth++], BCI_ERROR, "break_not_walkable");
            }
            return depth;
        }
    }

    // Walk until the bottom of the stack or until the first Java frame
    while (true) {
        if (depth == max_depth) {
            *truncated = true;
            break;
        }
        if (CodeHeap::contains(pc)) {
            NMethod* nm = CodeHeap::findNMethod(pc);
            if (nm == NULL) {
                fillFrame(frames[depth++], BCI_ERROR, "unknown_nmethod");
            } else if (nm->isNMethod()) {
                int level = nm->level();
                FrameTypeId type = detail != VM_BASIC && level >= 1 && level <= 3 ? FRAME_C1_COMPILED : FRAME_JIT_COMPILED;
                fillFrame(frames[depth++], type, 0, nm->method()->id());

                if (nm->isFrameCompleteAt(pc)) {
                    int scope_offset = nm->findScopeOffset(pc);
                    if (scope_offset > 0) {
                        depth--;
                        ScopeDesc scope(nm);
                        do {
                            scope_offset = scope.decode(scope_offset);
                            if (detail != VM_BASIC) {
                                type = scope_offset > 0 ? FRAME_INLINED :
                                       level >= 1 && level <= 3 ? FRAME_C1_COMPILED : FRAME_JIT_COMPILED;
                            }
                            fillFrame(frames[depth++], type, scope.bci(), scope.method()->id());
                        } while (scope_offset > 0 && depth < max_depth);
                    }

                    // Handle situations when sp is temporarily changed in the compiled code
                    frame.adjustSP(nm->entry(), pc, sp);

                    sp += nm->frameSize() * sizeof(void*);
                    fp = ((uintptr_t*)sp)[-FRAME_PC_SLOT - 1];
                    pc = ((const void**)sp)[-FRAME_PC_SLOT];
                    continue;
                } else if (frame.unwindCompiled(nm, (uintptr_t&)pc, sp, fp) && profiler->isAddressInCode(pc)) {
                    continue;
                }

                fillFrame(frames[depth++], BCI_ERROR, "break_compiled");
                break;
            } else if (nm->isInterpreter()) {
                if (vm_thread != NULL && vm_thread->inDeopt()) {
                    *truncated = true;
                    fillFrame(frames[depth++], BCI_ERROR, "break_deopt");
                    break;
                }

                bool is_plausible_interpreter_frame = !inDeadZone((const void*)fp) && aligned(fp)
                    && sp > fp - MAX_INTERPRETER_FRAME_SIZE
                    && sp < fp + bcp_offset * sizeof(void*);

                if (is_plausible_interpreter_frame) {
                    VMMethod* method = ((VMMethod**)fp)[InterpreterFrame::method_offset];
                    jmethodID method_id = getMethodId(method);
                    if (method_id != NULL) {
                        const char* bytecode_start = method->bytecode();
                        const char* bcp = ((const char**)fp)[bcp_offset];
                        int bci = bytecode_start == NULL || bcp < bytecode_start ? 0 : bcp - bytecode_start;
                        fillFrame(frames[depth++], FRAME_INTERPRETED, bci, method_id);

                        sp = ((uintptr_t*)fp)[InterpreterFrame::sender_sp_offset];
                        pc = stripPointer(((void**)fp)[FRAME_PC_SLOT]);
                        fp = *(uintptr_t*)fp;
                        continue;
                    }
                }

                if (depth == 0) {
                    VMMethod* method = (VMMethod*)frame.method();
                    jmethodID method_id = getMethodId(method);
                    if (method_id != NULL) {
                        fillFrame(frames[depth++], FRAME_INTERPRETED, 0, method_id);

                        if (is_plausible_interpreter_frame) {
                            pc = stripPointer(((void**)fp)[FRAME_PC_SLOT]);
                            sp = frame.senderSP();
                            fp = *(uintptr_t*)fp;
                        } else {
                            pc = stripPointer(*(void**)sp);
                            sp = frame.senderSP();
                        }
                        continue;
                    }
                }

                *truncated = true;
                fillFrame(frames[depth++], BCI_ERROR, "break_interpreted");
                break;
            } else if (detail < VM_EXPERT && nm->isEntryFrame(pc)) {
                JavaFrameAnchor* anchor = JavaFrameAnchor::fromEntryFrame(fp);
                if (anchor == NULL) {
                    *truncated = true;
                    fillFrame(frames[depth++], BCI_ERROR, "break_entry_frame");
                    break;
                }
                uintptr_t prev_sp = sp;
                sp = anchor->lastJavaSP();
                fp = anchor->lastJavaFP();
                pc = anchor->lastJavaPC();
                if (sp == 0 || pc == NULL) {
                    // End of Java stack
                    break;
                }
                if (sp < prev_sp || sp >= bottom || !aligned(sp)) {
                    *truncated = true;
                    fillFrame(frames[depth++], BCI_ERROR, "break_entry_frame");
                    break;
                }
                continue;
            } else {
                CodeBlob* stub = profiler->findRuntimeStub(pc);
                const void* start = stub != NULL ? stub->_start : nm->code();
                const char* name = stub != NULL ? stub->_name : nm->name();

                if (detail != VM_BASIC) {
                    fillFrame(frames[depth++], BCI_NATIVE_FRAME, name);
                }

                if (frame.unwindStub((instruction_t*)start, name, (uintptr_t&)pc, sp, fp)) {
                    continue;
                }

                if (depth > 1 && nm->frameSize() > 0) {
                    sp += nm->frameSize() * sizeof(void*);
                    fp = ((uintptr_t*)sp)[-FRAME_PC_SLOT - 1];
                    pc = ((const void**)sp)[-FRAME_PC_SLOT];
                    continue;
                }
            }
        } else {
            fillFrame(frames[depth++], BCI_NATIVE_FRAME, profiler->findNativeMethod(pc));
        }

        uintptr_t prev_sp = sp;
        CodeCache* cc = profiler->findLibraryByAddress(pc);
        FrameDesc* f = cc != NULL ? cc->findFrameDesc(pc) : &FrameDesc::default_frame;

        u8 cfa_reg = (u8)f->cfa;
        int cfa_off = f->cfa >> 8;
        if (cfa_reg == DW_REG_SP) {
            sp = sp + cfa_off;
        } else if (cfa_reg == DW_REG_FP) {
            sp = fp + cfa_off;
        } else if (cfa_reg == DW_REG_PLT) {
            sp += ((uintptr_t)pc & 15) >= 11 ? cfa_off * 2 : cfa_off;
        } else {
            break;
        }

        // Check if the next frame is below on the current stack
        if (sp < prev_sp || sp >= prev_sp + MAX_FRAME_SIZE || sp >= bottom) {
            break;
        }

        // Stack pointer must be word aligned
        if (!aligned(sp)) {
            *truncated = true;
            break;
        }

        if (f->fp_off & DW_PC_OFFSET) {
            pc = (const char*)pc + (f->fp_off >> 1);
        } else {
            if (f->fp_off != DW_SAME_FP && f->fp_off < MAX_FRAME_SIZE && f->fp_off > -MAX_FRAME_SIZE) {
                fp = *(uintptr_t*)(sp + f->fp_off);
            }
            if (EMPTY_FRAME_SIZE > 0 || cfa_off != 0) {
                // x86 or AArch64 non-default frame
                pc = stripPointer(*(void**)(sp + f->pc_off));
            } else if (f->fp_off != DW_SAME_FP) {
                // AArch64 default_frame
                pc = stripPointer(*(void**)(sp + f->pc_off));
                sp = defaultSenderSP(sp, fp);
                if (sp < prev_sp || sp >= bottom || !aligned(sp)) {
                    *truncated = true;
                    break;
                }
            } else if (depth <= 1) {
                pc = (const void*)frame.link();
            } else {
                // Stack bottom
                break;
            }
        }

        if (inDeadZone(pc)) {
            *truncated = pc != NULL;
            break;
        }
    }

    if (vm_thread != NULL) vm_thread->exception() = saved_exception;

    return depth;
}

void StackWalker::checkFault() {
    if (VMThread::key() < 0) {
        // JVM has not been loaded or VMStructs have not been initialized yet
        return;
    }

    VMThread* vm_thread = VMThread::current();
    if (vm_thread != NULL && sameStack(vm_thread->exception(), &vm_thread)) {
        longjmp(*(jmp_buf*)vm_thread->exception(), 1);
    }
}
