/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jvmti.h>
#include "tsc.h"
#include "vmEntry.h"
#include "vmStructs.h"


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
            _available = syncWithJvm();
        } else {
            _available = cpuHasGoodTimestampCounter();
        }
        _initialized = true;
    }

    _enabled = _available;
}

// Try to use the same clock source with the same offset/frequency as the JVM does
bool TSC::syncWithJvm() {
    JVMFlag* f = JVMFlag::find("UseFastUnorderedTimeStamps");
    if (f == nullptr || !f->get()) {
        return false;
    }

    JNIEnv* env = VM::jni();

    jfieldID jvm;
    jmethodID counterTime, getTicksFrequency;
    jclass cls = env->FindClass("jdk/jfr/internal/JVM");
    if (cls == nullptr || (counterTime = env->GetStaticMethodID(cls, "counterTime", "()J")) == nullptr) {
        env->ExceptionClear();
        return false;
    }

    u64 frequency = 0;
    // Method is static since JDK 22
    if ((getTicksFrequency = env->GetStaticMethodID(cls, "getTicksFrequency", "()J")) != nullptr) {
        frequency = env->CallStaticLongMethod(cls, getTicksFrequency);
    } else {
        env->ExceptionClear();
        if ((getTicksFrequency = env->GetMethodID(cls, "getTicksFrequency", "()J")) != nullptr &&
                (jvm = env->GetStaticFieldID(cls, "jvm", "Ljdk/jfr/internal/JVM;")) != nullptr) {
            frequency = env->CallLongMethod(env->GetStaticObjectField(cls, jvm), getTicksFrequency);
        }
    }
    env->ExceptionClear();

    if (frequency == 0 || !JFR_ALLOWED_FREQUENCY(frequency)) {
        return false;
    }

    u64 jvm_ticks = env->CallStaticLongMethod(cls, counterTime);
    _offset = rdtsc() - jvm_ticks;
    _frequency = frequency;

    env->ExceptionClear();
    return true;
}
