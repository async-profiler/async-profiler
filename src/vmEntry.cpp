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
#include <sys/mman.h>
#include "vmEntry.h"
#include "arguments.h"
#include "j9Ext.h"
#include "j9ObjectSampler.h"
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

static Arguments _agent_args(true);

JavaVM* VM::_vm;
jvmtiEnv* VM::_jvmti = NULL;

int VM::_hotspot_version = 0;
bool VM::_openj9 = false;
bool VM::_zing = false;
bool VM::_can_sample_objects = false;

jvmtiError (JNICALL *VM::_orig_RedefineClasses)(jvmtiEnv*, jint, const jvmtiClassDefinition*);
jvmtiError (JNICALL *VM::_orig_RetransformClasses)(jvmtiEnv*, jint, const jclass* classes);

void* VM::_libjvm;
void* VM::_libjava;
AsyncGetCallTrace VM::_asyncGetCallTrace;
JVM_GetManagement VM::_getManagement;


static void wakeupHandler(int signo) {
    // Dummy handler for interrupting syscalls
}

static bool isZeroInterpreterMethod(const char* blob_name) {
    return strncmp(blob_name, "_ZN15ZeroInterpreter", 20) == 0
        || strncmp(blob_name, "_ZN19BytecodeInterpreter3run", 28) == 0;
}

static bool isOpenJ9InterpreterMethod(const char* blob_name) {
    return strncmp(blob_name, "_ZN32VM_BytecodeInterpreter", 27) == 0
        || strncmp(blob_name, "_ZN26VM_BytecodeInterpreter", 27) == 0
        || strncmp(blob_name, "bytecodeLoop", 12) == 0
        || strcmp(blob_name, "cInterpreter") == 0;
}

static bool isOpenJ9JitStub(const char* blob_name) {
    if (strncmp(blob_name, "jit", 3) == 0) {
        blob_name += 3;
        return strcmp(blob_name, "NewObject") == 0
            || strcmp(blob_name, "NewArray") == 0
            || strcmp(blob_name, "ANewArray") == 0;
    }
    return false;
}

static void* resolveMethodId(void** mid) {
    return mid == NULL || *mid < (void*)4096 ? NULL : *mid;
}

static void resolveMethodIdEnd() {
}


bool VM::init(JavaVM* vm, bool attach) {
    if (_jvmti != NULL) return true;

    _vm = vm;
    if (_vm->GetEnv((void**)&_jvmti, JVMTI_VERSION_1_0) != 0) {
        return false;
    }

    Dl_info dl_info;
    if (dladdr((const void*)wakeupHandler, &dl_info) && dl_info.dli_fname != NULL) {
        // Make sure async-profiler DSO cannot be unloaded, since it contains JVM callbacks.
        // Don't use ELF NODELETE flag because of https://sourceware.org/bugzilla/show_bug.cgi?id=20839
        dlopen(dl_info.dli_fname, RTLD_LAZY | RTLD_NODELETE);
    }

    bool is_hotspot = false;
    bool is_zero_vm = false;
    char* prop;
    if (_jvmti->GetSystemProperty("java.vm.name", &prop) == 0) {
        is_hotspot = strstr(prop, "OpenJDK") != NULL ||
                     strstr(prop, "HotSpot") != NULL ||
                     strstr(prop, "GraalVM") != NULL ||
                     strstr(prop, "Dynamic Code Evolution") != NULL;
        is_zero_vm = strstr(prop, "Zero") != NULL;
        _zing = !is_hotspot && strstr(prop, "Zing") != NULL;
        _jvmti->Deallocate((unsigned char*)prop);
    }

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

    _libjvm = getLibraryHandle("libjvm.so");
    _asyncGetCallTrace = (AsyncGetCallTrace)dlsym(_libjvm, "AsyncGetCallTrace");
    _getManagement = (JVM_GetManagement)dlsym(_libjvm, "JVM_GetManagement");

    Profiler* profiler = Profiler::instance();
    profiler->updateSymbols(false);

    _openj9 = !is_hotspot && J9Ext::initialize(_jvmti, profiler->resolveSymbol("j9thread_self"));
    _can_sample_objects = !is_hotspot || hotspot_version() >= 11;

    CodeCache* lib = isOpenJ9()
        ? profiler->findJvmLibrary("libj9vm")
        : profiler->findLibraryByAddress((const void*)_asyncGetCallTrace);
    if (lib == NULL) {
        return false;
    }

    VMStructs::init(lib);
    if (is_zero_vm) {
        lib->mark(isZeroInterpreterMethod);
    } else if (isOpenJ9()) {
        lib->mark(isOpenJ9InterpreterMethod);
        CodeCache* libjit = profiler->findJvmLibrary("libj9jit");
        if (libjit != NULL) {
            libjit->mark(isOpenJ9JitStub);
        }
    }

    if (!attach && hotspot_version() == 8 && OS::isLinux()) {
        // Workaround for JDK-8185348
        char* func = (char*)lib->findSymbol("_ZN6Method26checked_resolve_jmethod_idEP10_jmethodID");
        if (func != NULL) {
            applyPatch(func, (const char*)resolveMethodId, (const char*)resolveMethodIdEnd);
        }
    }

    if (attach) {
        ready();
    }

    jvmtiCapabilities capabilities = {0};
    capabilities.can_generate_all_class_hook_events = 1;
    capabilities.can_retransform_classes = 1;
    capabilities.can_retransform_any_class = isOpenJ9() ? 0 : 1;
    capabilities.can_generate_vm_object_alloc_events = isOpenJ9() ? 1 : 0;
    capabilities.can_generate_sampled_object_alloc_events = _can_sample_objects ? 1 : 0;
    capabilities.can_get_bytecodes = 1;
    capabilities.can_get_constant_pool = 1;
    capabilities.can_get_source_file_name = 1;
    capabilities.can_get_line_numbers = 1;
    capabilities.can_generate_compiled_method_load_events = 1;
    capabilities.can_generate_monitor_events = 1;
    capabilities.can_tag_objects = 1;
    if (_jvmti->AddCapabilities(&capabilities) != 0) {
        _can_sample_objects = false;
        capabilities.can_generate_sampled_object_alloc_events = 0;
        _jvmti->AddCapabilities(&capabilities);
    }

    jvmtiEventCallbacks callbacks = {0};
    callbacks.VMInit = VMInit;
    callbacks.VMDeath = VMDeath;
    callbacks.ClassLoad = ClassLoad;
    callbacks.ClassPrepare = ClassPrepare;
    callbacks.ClassFileLoadHook = Instrument::ClassFileLoadHook;
    callbacks.CompiledMethodLoad = Profiler::CompiledMethodLoad;
    callbacks.DynamicCodeGenerated = Profiler::DynamicCodeGenerated;
    callbacks.ThreadStart = Profiler::ThreadStart;
    callbacks.ThreadEnd = Profiler::ThreadEnd;
    callbacks.MonitorContendedEnter = LockTracer::MonitorContendedEnter;
    callbacks.MonitorContendedEntered = LockTracer::MonitorContendedEntered;
    callbacks.VMObjectAlloc = J9ObjectSampler::VMObjectAlloc;
    callbacks.SampledObjectAlloc = ObjectSampler::SampledObjectAlloc;
    _jvmti->SetEventCallbacks(&callbacks, sizeof(callbacks));

    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_DEATH, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_LOAD, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_CLASS_PREPARE, NULL);
    _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL);

    if (hotspot_version() == 0 || !CodeHeap::available()) {
        // Workaround for JDK-8173361: avoid CompiledMethodLoad events when possible
        _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, NULL);
    } else {
        // DebugNonSafepoints is automatically enabled with CompiledMethodLoad,
        // otherwise we set the flag manually
        char* flag_addr = (char*)JVMFlag::find("DebugNonSafepoints");
        if (flag_addr != NULL) {
            *flag_addr = 1;
        }
    }

    if (attach) {
        loadAllMethodIDs(jvmti(), jni());
        _jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
        _jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
    } else {
        _jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_VM_INIT, NULL);
    }

    OS::installSignalHandler(WAKEUP_SIGNAL, NULL, wakeupHandler);

    return true;
}

// Run late initialization when JVM is ready
void VM::ready() {
    {
        JitWriteProtection jit(true);
        VMStructs::ready();
    }

    Profiler::setupSignalHandlers();

    _libjava = getLibraryHandle("libjava.so");

    // Make sure we reload method IDs upon class retransformation
    JVMTIFunctions* functions = *(JVMTIFunctions**)_jvmti;
    _orig_RedefineClasses = functions->RedefineClasses;
    _orig_RetransformClasses = functions->RetransformClasses;
    functions->RedefineClasses = RedefineClassesHook;
    functions->RetransformClasses = RetransformClassesHook;
}

void VM::applyPatch(char* func, const char* patch, const char* end_patch) {
    size_t size = end_patch - patch;
    uintptr_t start_page = (uintptr_t)func & ~OS::page_mask;
    uintptr_t end_page = ((uintptr_t)func + size + OS::page_mask) & ~OS::page_mask;

    if (mprotect((void*)start_page, end_page - start_page, PROT_READ | PROT_WRITE | PROT_EXEC) == 0) {
        memcpy(func, patch, size);
        __builtin___clear_cache(func, func + size);
        mprotect((void*)start_page, end_page - start_page, PROT_READ | PROT_EXEC);
    }
}

void* VM::getLibraryHandle(const char* name) {
    if (OS::isLinux()) {
        void* handle = dlopen(name, RTLD_LAZY);
        if (handle != NULL) {
            return handle;
        }
        Log::warn("Failed to load %s: %s", name, dlerror());
    }
    // JVM symbols are globally visible on macOS
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

void VM::restartProfiler() {
    Profiler::instance()->restart(_agent_args);
}

void JNICALL VM::VMInit(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread) {
    ready();
    loadAllMethodIDs(jvmti, jni);

    // Allow profiler server only at JVM startup
    if (_agent_args._server != NULL) {
        if (JavaAPI::startHttpServer(jvmti, jni, _agent_args._server)) {
            Log::info("Profiler server started at %s", _agent_args._server);
        } else {
            Log::error("Failed to start profiler server");
        }
    }

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

    return result;
}

jvmtiError VM::RetransformClassesHook(jvmtiEnv* jvmti, jint class_count, const jclass* classes) {
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

    return result;
}


extern "C" DLLEXPORT jint JNICALL
Agent_OnLoad(JavaVM* vm, char* options, void* reserved) {
    Error error = _agent_args.parse(options);

    Log::open(_agent_args);

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

extern "C" DLLEXPORT jint JNICALL
Agent_OnAttach(JavaVM* vm, char* options, void* reserved) {
    Arguments args(true);
    Error error = args.parse(options);

    Log::open(args);

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

extern "C" DLLEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved) {
    if (!VM::init(vm, true)) {
        return 0;
    }

    JavaAPI::registerNatives(VM::jvmti(), VM::jni());
    return JNI_VERSION_1_6;
}

extern "C" DLLEXPORT void JNICALL
JNI_OnUnload(JavaVM* vm, void* reserved) {
    Profiler* profiler = Profiler::instance();
    if (profiler != NULL) {
        profiler->stop();
    }
}
