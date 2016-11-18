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
#include <string.h>
#include "asyncProfiler.h"
#include "vmEntry.h"

JavaVM* VM::_vm;
jvmtiEnv* VM::_jvmti;

template<class FunctionType>
inline FunctionType getJvmFunction(const char *function_name) {
    // get address of function, return null if not found
    return (FunctionType) dlsym(RTLD_DEFAULT, function_name);
}

void VM::init(JavaVM* vm) {
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
    VM::init(vm);
    asgct = getJvmFunction<ASGCTType>("AsyncGetCallTrace");
    Profiler::_instance.start(DEFAULT_INTERVAL);
    return 0;
}

const char OPTION_DELIMITER[] = ",";
const char MAX_FRAMES[] = "maxFrames:";
const char START[] = "start";
const char STOP[] = "stop";
const char DUMP_RAW_TRACES[] = "dumpRawTraces:";

extern "C" JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    VM::attach(vm);
    asgct = getJvmFunction<ASGCTType>("AsyncGetCallTrace");

    char *token = strtok(options, OPTION_DELIMITER);
    while (token) {
        if (strncmp(token, MAX_FRAMES, strlen(MAX_FRAMES)) == 0) {
            const char *text = token + strlen(MAX_FRAMES);
            const int value = atoi(text);
            if (value >= 0) {
                std::cout << "Setting max frames to " << value << std::endl;
                maxFrames = value;
            } else {
                std::cout << "Ignoring max frames value " << value << std::endl;
            }
        } else if (strcmp(token, START) == 0) {
            std::cout << "Profiling started" << std::endl;
            Profiler::_instance.start(DEFAULT_INTERVAL);
        } else if (strcmp(token, STOP) == 0) {
            std::cout << "Profiling stopped" << std::endl;
            Profiler::_instance.stop();
            Profiler::_instance.dumpTraces(std::cout, DEFAULT_TRACES_TO_DUMP);
            Profiler::_instance.dumpMethods(std::cout);
        } else if (strncmp(token, DUMP_RAW_TRACES, strlen(DUMP_RAW_TRACES)) == 0) {
            std::cout << "Profiling stopped" << std::endl;
            Profiler::_instance.stop();

            const char *fileName = token + strlen(DUMP_RAW_TRACES);

            std::ofstream dump(fileName, std::ios::out | std::ios::trunc);
            if (!dump.is_open()) {
                std::cerr << "Couldn't open: " << fileName << std::endl;
                return -1;
            }

            std::cout << "Dumping raw traces to " << fileName << std::endl;
            Profiler::_instance.dumpRawTraces(dump);
            dump.close();
        }
        
        token = strtok(NULL, OPTION_DELIMITER);
    }
    
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    VM::attach(vm);
    asgct = getJvmFunction<ASGCTType>("AsyncGetCallTrace");
    return JNI_VERSION_1_6;
}
