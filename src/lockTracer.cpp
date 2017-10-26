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

#include "lockTracer.h"
#include "profiler.h"
#include "vmStructs.h"


jlong LockTracer::_start_time = 0;

Error LockTracer::start(const char* event, long interval) {
    NativeCodeCache* libjvm = Profiler::_instance.jvmLibrary();
    if (libjvm == NULL) {
        return Error("libjvm not found among loaded libraries");
    }

    if (!VMStructs::init(libjvm)) {
        return Error("VMStructs unavailable. Unsupported JVM?");
    }

    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);

    jvmti->GetTime(&_start_time);

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
    if (enter_time < _start_time) {
        return;
    }

    jclass lock_class = env->GetObjectClass(object);
    VMKlass* klass = (*(java_lang_Class**)lock_class)->klass();
    Profiler::_instance.recordSample(NULL, entered_time - enter_time, BCI_KLASS, (jmethodID)klass);
}
