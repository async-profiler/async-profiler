/*
 * Copyright 2017 Andrei Pangin
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

#include <dlfcn.h>
#include "lockTracer.h"
#include "profiler.h"
#include "vmStructs.h"


jlong LockTracer::_start_time = 0;
UnsafeParkFunc LockTracer::_real_unsafe_park = NULL;
jfieldID LockTracer::_thread_parkBlocker = NULL;

Error LockTracer::start(const char* event, long interval) {
    NativeCodeCache* libjvm = Profiler::_instance.jvmLibrary();
    if (libjvm == NULL) {
        return Error("libjvm not found among loaded libraries");
    }

    if (!VMStructs::init(libjvm)) {
        return Error("VMStructs unavailable. Unsupported JVM?");
    }

    // Enable Java Monitor events
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
    jvmti->GetTime(&_start_time);

    // Intercent Unsafe.park() for tracing contended ReentrantLocks
    if (_real_unsafe_park == NULL) {
        _real_unsafe_park = (UnsafeParkFunc)libjvm->findSymbol("Unsafe_Park");
    }

    JNIEnv* env = VM::jni();
    if (env != NULL && _real_unsafe_park != NULL) {
        // Cache Thread.parkBlocker field
        if (_thread_parkBlocker == NULL) {
            jclass thread = env->FindClass("java/lang/Thread");
            if (thread != NULL) {
                _thread_parkBlocker = env->GetFieldID(thread, "parkBlocker", "Ljava/lang/Object;");
                env->DeleteLocalRef(thread);
            }

            if (_thread_parkBlocker == NULL) {
                return Error::OK;
            }
        }

        // Try JDK 9+ package first, then fallback to JDK 8 package
        jclass unsafe = env->FindClass("jdk/internal/misc/Unsafe");
        if (unsafe == NULL) unsafe = env->FindClass("sun/misc/Unsafe");
        if (unsafe != NULL) {
            const JNINativeMethod unsafe_park = {(char*)"park", (char*)"(ZJ)V", (void*)LockTracer::UnsafePark};
            jint result = env->RegisterNatives(unsafe, &unsafe_park, 1);
            printf("Registered Unsafe native: %d\n", result);
            env->DeleteLocalRef(unsafe);
        }
    }

    return Error::OK;
}

void LockTracer::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
}

void JNICALL LockTracer::MonitorContendedEnter(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong enter_time;
    jvmti->GetTime(&enter_time);
    jvmti->SetTag(thread, enter_time);
}

void JNICALL LockTracer::MonitorContendedEntered(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong enter_time, entered_time;
    jvmti->GetTime(&entered_time);
    jvmti->GetTag(thread, &enter_time);

    // Time is meaningless if lock attempt has started before profiling
    if (enter_time >= _start_time) {
        recordContendedLock(env, entered_time - enter_time, object);
    }
}

void JNICALL LockTracer::UnsafePark(JNIEnv* env, jobject instance, jboolean isAbsolute, jlong time) {
    jobject park_blocker = NULL;
    jlong park_start_time, park_end_time;

    jvmtiEnv* jvmti = VM::jvmti();
    jthread thread;
    if (jvmti->GetCurrentThread(&thread) == 0) {
        park_blocker = env->GetObjectField(thread, _thread_parkBlocker);
    }

    if (park_blocker != NULL) {
        jvmti->GetTime(&park_start_time);
    }
    
    _real_unsafe_park(env, instance, isAbsolute, time);

    if (park_blocker != NULL) {
        jvmti->GetTime(&park_end_time);
        recordContendedLock(env, park_end_time - park_start_time, park_blocker);
    }
}

void LockTracer::recordContendedLock(JNIEnv* env, jlong time, jobject lock) {
    if (VMStructs::hasPermGen()) {
        // PermGen in JDK 7 makes difficult to get symbol name from jclass.
        // Let's just skip it and record stack trace without lock class.
        Profiler::_instance.recordSample(NULL, time, 0, NULL);
    } else {
        jclass lock_class = env->GetObjectClass(lock);
        VMSymbol* lock_name = (*(java_lang_Class**)lock_class)->klass()->name();
        Profiler::_instance.recordSample(NULL, time, BCI_SYMBOL, (jmethodID)lock_name);
    }
}
