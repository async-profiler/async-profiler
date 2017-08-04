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

#include <fstream>
#include <dlfcn.h>
#include "vmEntry.h"
#include "arguments.h"
#include "profiler.h"
#include "perfEvent.h"
#include "vmStructs.h"


JavaVM* VM::_vm;
jvmtiEnv* VM::_jvmti = NULL;
AsyncGetCallTrace VM::_asyncGetCallTrace;


bool VM::init(JavaVM* vm) {
    if (_jvmti != NULL) return true;

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
    callbacks.VMDeath = Profiler::VMDeath;
    callbacks.ClassLoad = ClassLoad;
    callbacks.ClassPrepare = ClassPrepare;
    callbacks.CompiledMethodLoad = Profiler::CompiledMethodLoad;
    callbacks.CompiledMethodUnload = Profiler::CompiledMethodUnload;
    callbacks.DynamicCodeGenerated = Profiler::DynamicCodeGenerated;
    callbacks.ThreadStart = PerfEvents::ThreadStart;
    callbacks.ThreadEnd = PerfEvents::ThreadEnd;
    _jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));

    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL);

    PerfEvents::init();
    VMStructs::init();

    _asyncGetCallTrace = (AsyncGetCallTrace)dlsym(RTLD_DEFAULT, "AsyncGetCallTrace");
    if (_asyncGetCallTrace == NULL) {
        std::cerr << "Could not find AsyncGetCallTrace function" << std::endl;
        return false;
    }
    return true;
}

bool VM::attach(JavaVM* vm) {
    if (_jvmti != NULL) return true;
    
    if (!init(vm)) return false;

    loadAllMethodIDs(_jvmti);
    _jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
    _jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);

    return true;
}

void VM::loadMethodIDs(jvmtiEnv* jvmti, jclass klass) {
    jint method_count;
    jmethodID* methods;
    if (jvmti->GetClassMethods(klass, &method_count, &methods) == 0) {
        jvmti->Deallocate((unsigned char*)methods);
    }
}

void VM::loadAllMethodIDs(jvmtiEnv* jvmti) {
    jint class_count;
    jclass* classes;
    if (jvmti->GetLoadedClasses(&class_count, &classes) == 0) {
        for (int i = 0; i < class_count; i++) {
            loadMethodIDs(jvmti, classes[i]);
        }
        jvmti->Deallocate((unsigned char*)classes);
    }
}


extern "C" JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    if (!VM::init(vm)) {
        return -1;
    }

    Arguments args(options);
    if (args.error() != NULL) {
        std::cerr << args.error() << std::endl;
        return -1;
    }

    Profiler::_instance.run(args);
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    if (!VM::attach(vm)) {
        return -1;
    }

    Arguments args(options);
    if (args.error() != NULL) {
        std::cerr << args.error() << std::endl;
        return -1;
    }

    Profiler::_instance.run(args);
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    if (!VM::attach(vm)) {
        return -1;
    }
    return JNI_VERSION_1_6;
}
