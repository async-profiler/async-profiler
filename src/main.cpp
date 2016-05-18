/*
 * Copyright 2016 Andrei Pangin
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

#include <sstream>
#include <string.h>
#include "asyncProfiler.h"

JavaVM* _vm;
jvmtiEnv* _jvmti;


static void loadMethodIDs(jvmtiEnv* jvmti, jclass klass) {
    jint method_count;
    jmethodID* methods;
    if (jvmti->GetClassMethods(klass, &method_count, &methods) == 0) {
        jvmti->Deallocate((unsigned char*)methods);
    }
}

static void loadAllMethodIDs(jvmtiEnv* jvmti) {
    jint class_count;
    jclass* classes;
    if (jvmti->GetLoadedClasses(&class_count, &classes) == 0) {
        for (int i = 0; i < class_count; i++) {
            loadMethodIDs(jvmti, classes[i]);
        }
        jvmti->Deallocate((unsigned char*)classes);
    }
}

static void JNICALL VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    loadAllMethodIDs(jvmti);
}

static void JNICALL ClassLoad(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
    // Needed only for AsyncGetCallTrace support
}

static void JNICALL ClassPrepare(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jclass klass) {
    loadMethodIDs(jvmti, klass);
}

static void JNICALL CompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method,
                                       jint code_size, const void* code_addr,
                                       jint map_length, const jvmtiAddrLocationMap* map,
                                       const void* compile_info) {
    // Needed to enable DebugNonSafepoints info by default
}

static void initJvmti(JavaVM* vm) {
    _vm = vm;
    _vm->GetEnv((void**)&_jvmti, JVMTI_VERSION_1_0);

    jvmtiCapabilities capabilities = {0};
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_get_bytecodes = 1;
    capabilities.can_get_constant_pool = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_generate_compiled_method_load_events = 1;
    _jvmti->AddCapabilities(&capabilities);

    jvmtiEventCallbacks callbacks = {0};
    callbacks.VMInit = VMInit;
    callbacks.ClassLoad = ClassLoad;
    callbacks.ClassPrepare = ClassPrepare;
    callbacks.CompiledMethodLoad = CompiledMethodLoad;
    _jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));

    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
}


extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    initJvmti(vm);
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    initJvmti(vm);
    Profiler::_instance.start();
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    initJvmti(vm);
    loadAllMethodIDs(_jvmti);

    if (strcmp(options, "start") == 0) {
        std::cout << "Profiling started\n";
        Profiler::_instance.start();
    } else if (strcmp(options, "stop") == 0) {
        std::cout << "Profiling stopped\n";
        Profiler::_instance.stop();
        Profiler::_instance.dump(std::cout, MAX_TRACES_TO_DUMP);
    }

    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_start0(JNIEnv* env, jclass cls) {
    Profiler::_instance.start();
}

extern "C" JNIEXPORT void JNICALL
Java_one_profiler_AsyncProfiler_stop0(JNIEnv* env, jclass cls) {
    Profiler::_instance.stop();
}

extern "C" JNIEXPORT jstring JNICALL
Java_one_profiler_AsyncProfiler_dump0(JNIEnv* env, jclass cls, jint max_traces) {
    std::ostringstream out;
    Profiler::_instance.dump(out, max_traces);
    return env->NewStringUTF(out.str().c_str());
}
