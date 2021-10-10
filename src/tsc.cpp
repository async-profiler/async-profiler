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

#include <jvmti.h>
#include "tsc.h"
#include "vmEntry.h"


bool TSC::_initialized = false;
bool TSC::_enabled = false;
u64 TSC::_offset = 0;
u64 TSC::_frequency = 1000000000;


void TSC::initialize() {
    JNIEnv* env = VM::jni();

    jfieldID jvm;
    jmethodID getTicksFrequency, counterTime;
    jclass cls = env->FindClass("jdk/jfr/internal/JVM");
    if (cls != NULL
            && ((jvm = env->GetStaticFieldID(cls, "jvm", "Ljdk/jfr/internal/JVM;")) != NULL)
            && ((getTicksFrequency = env->GetMethodID(cls, "getTicksFrequency", "()J")) != NULL)
            && ((counterTime = env->GetStaticMethodID(cls, "counterTime", "()J")) != NULL)) {

        u64 frequency = env->CallLongMethod(env->GetStaticObjectField(cls, jvm), getTicksFrequency);
        if (frequency > 1000000000) {
            // Default 1GHz frequency might mean that rdtsc is not available
            u64 jvm_ticks = env->CallStaticLongMethod(cls, counterTime);
            _offset = rdtsc() - jvm_ticks;
            _frequency = frequency;
            _enabled = true;
        }
    }

    env->ExceptionClear();
    _initialized = true;
}
