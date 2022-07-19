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

#include "stackWalker.h"
#include "safeAccess.h"
#include "dwarf.h"
#include "profiler.h"
#include "safeAccess.h"
#include "stackFrame.h"
#include "vmStructs.h"


const intptr_t MIN_VALID_PC = 0x1000;
const intptr_t MAX_WALK_SIZE = 0x100000;
const intptr_t MAX_FRAME_SIZE = 0x40000;


int StackWalker::walkFP(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx, bool *truncated) {
    const void* pc;
    uintptr_t fp;
    uintptr_t sp;
    uintptr_t bottom = (uintptr_t)&sp + MAX_WALK_SIZE;

    if (ucontext == NULL) {
        pc = __builtin_return_address(0);
        fp = (uintptr_t)__builtin_frame_address(1);
        sp = (uintptr_t)__builtin_frame_address(0);
    } else {
        StackFrame frame(ucontext);
        pc = (const void*)frame.pc();
        fp = frame.fp();
        sp = frame.sp();
    }

    int depth = 0;

    *truncated = false;

    // Walk until the bottom of the stack or until the first Java frame
    while (true) {
        if (depth == max_depth) {
            *truncated = true;
            break;
        }
        if (CodeHeap::contains(pc)) {
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
        if ((fp & (sizeof(uintptr_t) - 1)) != 0) {
            *truncated = true;
            break;
        }

        pc = stripPointer(SafeAccess::load((void**)fp + FRAME_PC_SLOT));
        if (pc < (const void*)MIN_VALID_PC || pc > (const void*)-MIN_VALID_PC) {
            *truncated = pc != NULL;
            break;
        }

        sp = fp + (FRAME_PC_SLOT + 1) * sizeof(void*);
        fp = *(uintptr_t*)fp;
    }

    return depth;
}

int StackWalker::walkDwarf(void* ucontext, const void** callchain, int max_depth, StackContext* java_ctx, bool *truncated) {
    const void* pc;
    uintptr_t fp;
    uintptr_t sp;
    uintptr_t prev_sp;
    uintptr_t bottom = (uintptr_t)&sp + MAX_WALK_SIZE;
    
    if (ucontext == NULL) {
        pc = __builtin_return_address(0);
        fp = (uintptr_t)__builtin_frame_address(1);
        sp = (uintptr_t)__builtin_frame_address(0);
    } else {
        StackFrame frame(ucontext);
        pc = (const void*)frame.pc();
        fp = frame.fp();
        sp = frame.sp();
    }

    int depth = 0;
    Profiler* profiler = Profiler::instance();

    *truncated = false;

    // Walk until the bottom of the stack or until the first Java frame
    while (true) {
        if (depth == max_depth) {
            *truncated = true;
            break;
        }
        if (CodeHeap::contains(pc)) {
            java_ctx->set(pc, sp, fp);
            break;
        }

        callchain[depth++] = pc;
        prev_sp = sp;

        FrameDesc* f;
        CodeCache* cc = profiler->findLibraryByAddress(pc);
        if (cc == NULL || (f = cc->findFrameDesc(pc)) == NULL) {
            f = &FrameDesc::default_frame;
        }

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
        if ((sp & (sizeof(uintptr_t) - 1)) != 0) {
            *truncated = true;
            break;
        }

        if (f->fp_off & DW_PC_OFFSET) {
            pc = (const char*)pc + (f->fp_off >> 1);
        } else {
            if (f->fp_off != DW_SAME_FP && f->fp_off < MAX_FRAME_SIZE && f->fp_off > -MAX_FRAME_SIZE) {
                fp = (uintptr_t)SafeAccess::load((void**)(sp + f->fp_off));
            }
            pc = stripPointer(SafeAccess::load((void**)sp - 1));
        }

        if (pc < (const void*)MIN_VALID_PC || pc > (const void*)-MIN_VALID_PC) {
            *truncated = pc != NULL;
            break;
        }
    }

    return depth;
}
