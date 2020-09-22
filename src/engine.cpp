/*
 * Copyright 2018 Andrei Pangin
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

#include "engine.h"
#include "stackFrame.h"


Error Engine::check(Arguments& args) {
    return Error::OK;
}

CStack Engine::cstack() {
    return CSTACK_FP;
}

int Engine::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth,
                           CodeCache* java_methods, CodeCache* runtime_stubs) {
    if (ucontext == NULL) {
        return 0;
    }

    StackFrame frame(ucontext);
    const void* pc = (const void*)frame.pc();
    uintptr_t fp = frame.fp();
    uintptr_t prev_fp = (uintptr_t)&fp;
    uintptr_t bottom = prev_fp + 0x100000;

    int depth = 0;
    const void* const valid_pc = (const void*)0x1000;

    // Walk until the bottom of the stack or until the first Java frame
    while (depth < max_depth && pc >= valid_pc) {
        if (java_methods->contains(pc) || runtime_stubs->contains(pc)) {
            break;
        }

        callchain[depth++] = pc;

        // Check if the next frame is below on the current stack
        if (fp <= prev_fp || fp >= prev_fp + 0x40000 || fp >= bottom) {
            break;
        }

        // Frame pointer must be word aligned
        if ((fp & (sizeof(uintptr_t) - 1)) != 0) {
            break;
        }

        prev_fp = fp;
        pc = ((const void**)fp)[1];
        fp = ((uintptr_t*)fp)[0];
    }

    return depth;
}
