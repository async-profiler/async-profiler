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

#include <stdio.h>
#include "engine.h"
#include "stackFrame.h"


int Engine::getNativeTrace(void* ucontext, int tid, const void** callchain, int max_depth,
                           VmCodeCache *cc) {
    StackFrame frame(ucontext);
    const void* pc = (const void*)frame.pc();
    uintptr_t fp = frame.fp();
    uintptr_t prev_fp = (uintptr_t)&fp;

    int depth = 0;
    const void* const valid_pc = (const void*)0x1000;

    // Walk until the bottom of the stack or until the first Java frame
    while (depth < max_depth && pc >= valid_pc) {
        callchain[depth++] = pc;

        if (cc->contains(pc)) {
            break;
        }

        // Check if the next frame is below on the current stack
        if (fp <= prev_fp || fp >= prev_fp + 0x40000) {
            break;
        }

        prev_fp = fp;
        pc = ((const void**)fp)[1];
        fp = ((uintptr_t*)fp)[0];
    }

    return depth;
}
