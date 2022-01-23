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

#ifndef _J9STRUCTS_H
#define _J9STRUCTS_H

#include <jvmti.h>


struct jvmtiFrameInfoExtended {
    jmethodID method;
    jlocation location;
    jlocation machinepc;
    jint type;
    void* native_frame_address;
};

struct jvmtiStackInfoExtended {
    jthread thread;
    jint state;
    jvmtiFrameInfoExtended* frame_buffer;
    jint frame_count;
};

enum {
    SHOW_COMPILED_FRAMES = 4,
    SHOW_INLINED_FRAMES = 8
};

#endif // _J9STRUCTS_H
