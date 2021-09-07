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

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include "vmEntry.h"
#include "arguments.h"
#include "javaApi.h"
#include "os.h"
#include "profiler.h"
#include "instrument.h"
#include "lockTracer.h"
#include "log.h"
#include "vmStructs.h"


// JVM TI agent return codes
const int ARGUMENTS_ERROR = 100;
const int COMMAND_ERROR = 200;

static Arguments _agent_args;

JavaVM* VM::_vm;
jvmtiEnv* VM::_jvmti = NULL;
int VM::_hotspot_version = 0;
void* VM::_libjvm;
void* VM::_libjava;
AsyncGetCallTrace VM::_asyncGetCallTrace;
JVM_GetManagement VM::_getManagement;
jvmtiError (JNICALL *VM::_orig_RedefineClasses)(jvmtiEnv*, jint, const jvmtiClassDefinition*);
jvmtiError (JNICALL *VM::_orig_RetransformClasses)(jvmtiEnv*, jint, const jclass* classes);
jvmtiError (JNICALL *VM::_orig_GenerateEvents)(jvmtiEnv* jvmti, jvmtiEvent event_type);
volatile int VM::_in_redefine_classes = 0;


static void wakeupHandler(int signo) {
    // Dummy handler for interrupting syscalls
}


bool VM::init(JavaVM* vm, bool attach) {
    if (_jvmti != NULL) return true;

    _vm = vm;
    if (_vm->GetEnv((void**)&_jvmti, JVMTI_VERSION_1_0) != 0) {
        return false;
    }

    char* prop;
    if (_jvmti->GetSystemProperty("java.vm.name", &prop) == 0) {
        bool is_hotspot = strstr(prop, "OpenJDK") != NULL ||
                          strstr(prop, "HotSpot") != NULL ||
                          strstr(prop, "GraalVM") != NULL ||
                          strstr(prop, "Dynamic Code Evolution") != NULL;
        _jvmti->Deallocate((unsigned char*)prop);

        if (is_hotspot && _jvmti->GetSystemProperty("java.vm.version", &prop) == 0) {
            if (strncmp(prop, "25.", 3) == 0) {
                _hotspot_version = 8;
            } else if (strncmp(prop, "24.", 3) == 0) {
                _hotspot_version = 7;
            } else if (strncmp(prop, "20.", 3) == 0) {
                _hotspot_version = 6;
            } else if ((_hotspot_version = atoi(prop)) < 9) {
                _hotspot_version = 9;
            }
            _jvmti->Deallocate((unsigned char*)prop);
        }
    }

    _libjvm = getLibraryHandle("libjvm.so");
    _asyncGetCallTrace = (AsyncGetCallTrace)dlsym(_libjvm, "AsyncGetCallTrace");
    _getManagement = (JVM_GetManagement)dlsym(_libjvm, "JVM_GetManagement");

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
        DisableSweeper ds;
        _jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
        _jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
    }

    if (hotspot_version() > 0 && hotspot_version() < 11) {
        // Avoid GenerateEvents conflict with another agent
        JVMTIFunctions* functions = *(JVMTIFunctions**)_jvmti;
        _orig_GenerateEvents = functions->GenerateEvents;
        functions->GenerateEvents = GenerateEventsHook;
    }

    OS::installSignalHandler(WAKEUP_SIGNAL, NULL, wakeupHandler);

    return true;
}

// Run late initialization when JVM is ready
void VM::ready() {
    Profiler* profiler = Profiler::instance();
    profiler->updateSymbols(false);
    NativeCodeCache* libjvm = profiler->findNativeLibrary((const void*)_asyncGetCallTrace);
    if (libjvm != NULL) {
        JitWriteProtection jit(true);  // workaround for JDK-8262896
        VMStructs::init(libjvm);
    }

    profiler->setupTrapHandler();

    _libjava = getLibraryHandle("libjava.so");

    // Make sure we reload method IDs upon class retransformation
    JVMTIFunctions* functions = *(JVMTIFunctions**)_jvmti;
    _orig_RedefineClasses = functions->RedefineClasses;
    _orig_RetransformClasses = functions->RetransformClasses;
    functions->RedefineClasses = RedefineClassesHook;
    functions->RetransformClasses = RetransformClassesHook;
}

void* VM::getLibraryHandle(const char* name) {
    if (!OS::isJavaLibraryVisible()) {
        void* handle = dlopen(name, RTLD_LAZY);
        if (handle != NULL) {
            return handle;
        }
        Log::warn("Failed to load %s: %s", name, dlerror());
    }
    return RTLD_DEFAULT;
}

void VM::loadMethodIDs(jvmtiEnv* jvmti, JNIEnv* jni, jclass klass) {
    if (VMStructs::hasClassLoaderData()) {
        VMKlass* vmklass = VMKlass::fromJavaClass(jni, klass);
        int method_count = vmklass->methodCount();
        if (method_count > 0) {
            ClassLoaderData* cld = vmklass->classLoaderData();
            cld->lock();
            // Workaround for JVM bug: preallocate space for jmethodIDs
            // at the beginning of the list (rather than at the end)
            for (int i = 0; i < method_count; i += MethodList::SIZE) {
                *cld->methodList() = new MethodList(*cld->methodList());
            }
            cld->unlock();
        }
    }

    jint method_count;
    jmethodID* methods;
    if (jvmti->GetClassMethods(klass, &method_count, &methods) == 0) {
        jvmti->Deallocate((unsigned char*)methods);
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
    Error error = Profiler::instance()->run(_agent_args);
    if (error) {
        Log::error("%s", error.message());
    }
}

void JNICALL VM::VMDeath(jvmtiEnv* jvmti, JNIEnv* jni) {
    Profiler::instance()->shutdown(_agent_args);
}

jvmtiError VM::RedefineClassesHook(jvmtiEnv* jvmti, jint class_count, const jvmtiClassDefinition* class_definitions) {
    atomicInc(_in_redefine_classes);
    jvmtiError result = _orig_RedefineClasses(jvmti, class_count, class_definitions);

    if (result == 0) {
        // jmethodIDs are invalidated after RedefineClasses
        JNIEnv* env = jni();
        for (int i = 0; i < class_count; i++) {
            if (class_definitions[i].klass != NULL) {
                loadMethodIDs(jvmti, env, class_definitions[i].klass);
            }
        }
    }

    atomicInc(_in_redefine_classes, -1);
    return result;
}

jvmtiError VM::RetransformClassesHook(jvmtiEnv* jvmti, jint class_count, const jclass* classes) {
    atomicInc(_in_redefine_classes);
    jvmtiError result = _orig_RetransformClasses(jvmti, class_count, classes);

    if (result == 0) {
        // jmethodIDs are invalidated after RetransformClasses
        JNIEnv* env = jni();
        for (int i = 0; i < class_count; i++) {
            if (classes[i] != NULL) {
                loadMethodIDs(jvmti, env, classes[i]);
            }
        }
    }

    atomicInc(_in_redefine_classes, -1);
    return result;
}

jvmtiError VM::GenerateEventsHook(jvmtiEnv* jvmti, jvmtiEvent event_type) {
    if (event_type == JVMTI_EVENT_COMPILED_METHOD_LOAD) {
        // Workaround for JDK-8222072: prepare to receive events designated for another agent
        Log::warn("async-profiler conflicts with another agent calling GenerateEvents()");
        Profiler::instance()->resetJavaMethods();
    }
    return _orig_GenerateEvents(jvmti, event_type);
}


extern "C" JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    Error error = _agent_args.parse(options);
    Log::open(_agent_args._log);
    if (error) {
        Log::error("%s", error.message());
        return ARGUMENTS_ERROR;
    }

    if (!VM::init(vm, false)) {
        Log::error("JVM does not support Tool Interface");
        return COMMAND_ERROR;
    }

    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    Arguments args;
    Error error = args.parse(options);
    Log::open(args._log);
    if (error) {
        Log::error("%s", error.message());
        return ARGUMENTS_ERROR;
    }

    if (!VM::init(vm, true)) {
        Log::error("JVM does not support Tool Interface");
        return COMMAND_ERROR;
    }

    // Save the arguments in case of shutdown
    if (args._action == ACTION_START || args._action == ACTION_RESUME) {
        _agent_args.save(args);
    }

    error = Profiler::instance()->run(args);
    if (error) {
        Log::error("%s", error.message());
        return COMMAND_ERROR;
    }

    return 0;
}

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    if (!VM::init(vm, true)) {
        return 0;
    }

    JavaAPI::registerNatives(VM::jvmti(), VM::jni());
    return JNI_VERSION_1_6;
}
