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
#include <string.h>
#include "vmEntry.h"
#include "arguments.h"
#include "javaApi.h"
#include "os.h"
#include "profiler.h"
#include "instrument.h"
#include "lockTracer.h"
#include "vmStructs.h"


static Arguments _agent_args;

JavaVM* VM::_vm;
jvmtiEnv* VM::_jvmti = NULL;
int VM::_hotspot_version = 0;
void* VM::_libjvm;
void* VM::_libjava;
AsyncGetCallTrace VM::_asyncGetCallTrace;


void VM::init(JavaVM* vm, bool attach) {
    if (_jvmti != NULL) return;

    _vm = vm;
    _vm->GetEnv((void**)&_jvmti, JVMTI_VERSION_1_0);

    char* prop;
    if (_jvmti->GetSystemProperty("java.vm.name", &prop) == 0) {
        bool is_hotspot = strstr(prop, "OpenJDK") != NULL ||
                          strstr(prop, "HotSpot") != NULL ||
                          strstr(prop, "GraalVM") != NULL;
        _jvmti->Deallocate((unsigned char*)prop);

        if (is_hotspot && _jvmti->GetSystemProperty("java.vm.version", &prop) == 0) {
            _hotspot_version = strncmp(prop, "25.", 3) == 0 ? 8 :
                               strncmp(prop, "24.", 3) == 0 ? 7 :
                               strncmp(prop, "20.", 3) == 0 ? 6 : 9;
            _jvmti->Deallocate((unsigned char*)prop);
        }
    }

    _libjvm = getLibraryHandle("libjvm.so");
    _asyncGetCallTrace = (AsyncGetCallTrace)dlsym(_libjvm, "AsyncGetCallTrace");

    if (attach) {
        ready();
    }

    jvmtiCapabilities capabilities = {0};
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_retransform_classes = 1;
    capabilities.can_retransform_any_class = 1;
    capabilities.can_get_bytecodes = 1;
    capabilities.can_get_constant_pool = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_generate_compiled_method_load_events = 1;
    capabilities.can_generate_monitor_events = 1;
    capabilities.can_tag_objects = 1;
    _jvmti->AddCapabilities(&capabilities);

    jvmtiEventCallbacks callbacks = {0};
    callbacks.VMInit = VMInit;
    callbacks.VMDeath = VMDeath;
    callbacks.ClassLoad = ClassLoad;
    callbacks.ClassPrepare = ClassPrepare;
    callbacks.ClassFileLoadHook = Instrument::ClassFileLoadHook;
    callbacks.CompiledMethodLoad = Profiler::CompiledMethodLoad;
    callbacks.CompiledMethodUnload = Profiler::CompiledMethodUnload;
    callbacks.DynamicCodeGenerated = Profiler::DynamicCodeGenerated;
    callbacks.ThreadStart = Profiler::ThreadStart;
    callbacks.ThreadEnd = Profiler::ThreadEnd;
    callbacks.MonitorContendedEnter = LockTracer::MonitorContendedEnter;
    callbacks.MonitorContendedEntered = LockTracer::MonitorContendedEntered;
    _jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));

    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL);

    if (attach) {
        loadAllMethodIDs(jvmti(), jni());
        _jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
        _jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
    }
}

// Run late initialization when JVM is ready
void VM::ready() {
    Profiler::_instance.updateSymbols();
    NativeCodeCache* libjvm = Profiler::_instance.findNativeLibrary((const void*)_asyncGetCallTrace);
    if (libjvm != NULL) {
        VMStructs::init(libjvm);
    }

    _libjava = getLibraryHandle("libjava.so");
}

void* VM::getLibraryHandle(const char* name) {
    if (!OS::isJavaLibraryVisible()) {
        void* handle = dlopen(name, RTLD_LAZY);
        if (handle != NULL) {
            return handle;
        }
        std::cerr << "Failed to load " << name << ": " << dlerror() << std::endl;
    }
    return RTLD_DEFAULT;
}

void VM::loadMethodIDs(jvmtiEnv* jvmti, JNIEnv* jni, jclass klass) {
    ClassLoaderData* cld;
    MethodList* old_list = NULL;

    if (VMStructs::hasClassLoaderData()) {
        cld = VMKlass::fromJavaClass(jni, klass)->classLoaderData();
        cld->lock();
        old_list = *cld->methodList();
        *cld->methodList() = NULL;
        cld->unlock();
    }

    jint method_count;
    jmethodID* methods;
    if (jvmti->GetClassMethods(klass, &method_count, &methods) == 0) {
        jvmti->Deallocate((unsigned char*)methods);
    }

    if (old_list != NULL) {
        cld->lock();
        MethodList** tail = cld->methodList();
        while (*tail != NULL) {
            tail = &(*tail)->next;
        }
        *tail = old_list;
        cld->unlock();
    }
}

void VM::loadAllMethodIDs(jvmtiEnv* jvmti, JNIEnv* jni) {
    jint class_count;
    jclass* classes;
    if (jvmti->GetLoadedClasses(&class_count, &classes) == 0) {
        for (int i = 0; i < class_count; i++) {
            loadMethodIDs(jvmti, jni, classes[i]);
        }
        jvmti->Deallocate((unsigned char*)classes);
    }
}

void JNICALL VM::VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    ready();
    loadAllMethodIDs(jvmti, jni);

    // Delayed start of profiler if agent has been loaded at VM bootstrap
    Profiler::_instance.run(_agent_args);
}

void JNICALL VM::VMDeath(jvmtiEnv* jvmti, JNIEnv* jni) {
    Profiler::_instance.shutdown(_agent_args);
}


extern "C" JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    VM::init(vm, false);

    Error error = _agent_args.parse(options);
    if (error) {
        std::cerr << error.message() << std::endl;
        return -1;
    }

    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    VM::init(vm, true);

    Arguments args;
    Error error = args.parse(options);
    if (error) {
        std::cerr << error.message() << std::endl;
        return -1;
    }

    // Save the arguments in case of shutdown
    if (args._action == ACTION_START || args._action == ACTION_RESUME) {
        _agent_args.save(args);
    }
    Profiler::_instance.run(args);

    return 0;
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    VM::init(vm, true);
    JavaAPI::registerNatives(VM::jvmti(), VM::jni());
    return JNI_VERSION_1_6;
}
