/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jvmti.h>
#include "tsc.h"
#include "vmEntry.h"


bool TSC::_initialized = false;
bool TSC::_available = false;
bool TSC::_enabled = false;
u64 TSC::_offset = 0;
u64 TSC::_frequency = NANOTIME_FREQ;

void TSC::enable(Clock clock) {
    if (!TSC_SUPPORTED || clock == CLK_MONOTONIC) {
        _enabled = false;
        return;
    }

    if (!_initialized) {
        if (VM::loaded()) {
            JNIEnv* env = VM::jni();

            jfieldID jvm;
            jmethodID getTicksFrequency, counterTime;
            jclass cls = env->FindClass("jdk/jfr/internal/JVM");
            if (cls != NULL
                    && ((jvm = env->GetStaticFieldID(cls, "jvm", "Ljdk/jfr/internal/JVM;")) != NULL)
                    && ((getTicksFrequency = env->GetMethodID(cls, "getTicksFrequency", "()J")) != NULL)
                    && ((counterTime = env->GetStaticMethodID(cls, "counterTime", "()J")) != NULL)) {
                u64 frequency = env->CallLongMethod(env->GetStaticObjectField(cls, jvm), getTicksFrequency);
                if (frequency > NANOTIME_FREQ) {
                    // Default 1GHz frequency might mean that rdtsc is not available
                    u64 jvm_ticks = env->CallStaticLongMethod(cls, counterTime);
                    _offset = rdtsc() - jvm_ticks;
                    _frequency = frequency;
                    _available = true;
                }
            }

            env->ExceptionClear();
        } else if (cpuHasGoodTimestampCounter()) {
            _offset = 0;
            _available = true;
        }

        _initialized = true;
    }

    _enabled = _available;
}
