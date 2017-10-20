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


Error LockTracer::start() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);

    return Error::OK;
}

void LockTracer::stop() {
    jvmtiEnv* jvmti = VM::jvmti();
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTER, NULL);
    jvmti->SetEventNotificationMode(JVMTI_DISABLE, JVMTI_EVENT_MONITOR_CONTENDED_ENTERED, NULL);
}

void JNICALL LockTracer::MonitorContendedEnter(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong begin_time;
    jvmti->GetTime(&begin_time);
    jvmti->SetTag(thread, begin_time);
}

void JNICALL LockTracer::MonitorContendedEntered(jvmtiEnv* jvmti, JNIEnv* env, jthread thread, jobject object) {
    jlong begin_time, end_time;
    jvmti->GetTime(&end_time);
    jvmti->GetTag(thread, &begin_time);

    Profiler::_instance.recordSample(NULL, end_time - begin_time, 0, NULL);
}
