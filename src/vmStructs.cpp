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

#include <pthread.h>
#include <unistd.h>
#include "vmStructs.h"
#include "vmEntry.h"
#include "j9Ext.h"


CodeCache* VMStructs::_libjvm = NULL;

bool VMStructs::_has_class_names = false;
bool VMStructs::_has_method_structs = false;
bool VMStructs::_has_class_loader_data = false;
bool VMStructs::_has_native_thread_id = false;
bool VMStructs::_has_perm_gen = false;

int VMStructs::_klass_name_offset = -1;
int VMStructs::_symbol_length_offset = -1;
int VMStructs::_symbol_length_and_refcount_offset = -1;
int VMStructs::_symbol_body_offset = -1;
int VMStructs::_class_loader_data_offset = -1;
int VMStructs::_class_loader_data_next_offset = -1;
int VMStructs::_methods_offset = -1;
int VMStructs::_jmethod_ids_offset = -1;
int VMStructs::_thread_osthread_offset = -1;
int VMStructs::_thread_anchor_offset = -1;
int VMStructs::_thread_state_offset = -1;
int VMStructs::_osthread_id_offset = -1;
int VMStructs::_anchor_sp_offset = -1;
int VMStructs::_anchor_pc_offset = -1;
int VMStructs::_frame_size_offset = -1;
int VMStructs::_frame_complete_offset = -1;
int VMStructs::_nmethod_name_offset = -1;
int VMStructs::_nmethod_method_offset = -1;
int VMStructs::_nmethod_entry_offset = -1;
int VMStructs::_nmethod_state_offset = -1;
int VMStructs::_nmethod_level_offset = -1;
int VMStructs::_method_constmethod_offset = -1;
int VMStructs::_method_code_offset = -1;
int VMStructs::_constmethod_constants_offset = -1;
int VMStructs::_constmethod_idnum_offset = -1;
int VMStructs::_pool_holder_offset = -1;
int VMStructs::_array_data_offset = -1;
int VMStructs::_code_heap_memory_offset = -1;
int VMStructs::_code_heap_segmap_offset = -1;
int VMStructs::_code_heap_segment_shift = -1;
int VMStructs::_vs_low_bound_offset = -1;
int VMStructs::_vs_high_bound_offset = -1;
int VMStructs::_vs_low_offset = -1;
int VMStructs::_vs_high_offset = -1;
int VMStructs::_flag_name_offset = -1;
int VMStructs::_flag_addr_offset = -1;
const char* VMStructs::_flags_addr = NULL;
int VMStructs::_flag_count = 0;
int VMStructs::_flag_size = 0;
char* VMStructs::_code_heap[3] = {};
const void* VMStructs::_code_heap_low = NO_MIN_ADDRESS;
const void* VMStructs::_code_heap_high = NO_MAX_ADDRESS;
char** VMStructs::_code_heap_addr = NULL;
const void** VMStructs::_code_heap_low_addr = NULL;
const void** VMStructs::_code_heap_high_addr = NULL;
int* VMStructs::_klass_offset_addr = NULL;

jfieldID VMStructs::_eetop;
jfieldID VMStructs::_tid;
jfieldID VMStructs::_klass = NULL;
int VMStructs::_tls_index = -1;
intptr_t VMStructs::_env_offset;

VMStructs::GetStackTraceFunc VMStructs::_get_stack_trace = NULL;
VMStructs::LockFunc VMStructs::_lock_func;
VMStructs::LockFunc VMStructs::_unlock_func;


uintptr_t VMStructs::readSymbol(const char* symbol_name) {
    const void* symbol = _libjvm->findSymbol(symbol_name);
    if (symbol == NULL) {
        // Avoid JVM crash in case of missing symbols
        return 0;
    }
    return *(uintptr_t*)symbol;
}

// Run at agent load time
void VMStructs::init(CodeCache* libjvm) {
    _libjvm = libjvm;

    initOffsets();
    initJvmFunctions();
}

// Run when VM is initialized and JNI is available
void VMStructs::ready() {
    resolveOffsets();

    JNIEnv* env = VM::jni();
    initThreadBridge(env);
    initLogging(env);
}

void VMStructs::initOffsets() {
    uintptr_t entry = readSymbol("gHotSpotVMStructs");
    uintptr_t stride = readSymbol("gHotSpotVMStructEntryArrayStride");
    uintptr_t type_offset = readSymbol("gHotSpotVMStructEntryTypeNameOffset");
    uintptr_t field_offset = readSymbol("gHotSpotVMStructEntryFieldNameOffset");
    uintptr_t offset_offset = readSymbol("gHotSpotVMStructEntryOffsetOffset");
    uintptr_t address_offset = readSymbol("gHotSpotVMStructEntryAddressOffset");

    if (entry == 0 || stride == 0) {
        return;
    }

    for (;; entry += stride) {
        const char* type = *(const char**)(entry + type_offset);
        const char* field = *(const char**)(entry + field_offset);
        if (type == NULL || field == NULL) {
            break;
        }

        if (strcmp(type, "Klass") == 0) {
            if (strcmp(field, "_name") == 0) {
                _klass_name_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "Symbol") == 0) {
            if (strcmp(field, "_length") == 0) {
                _symbol_length_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_length_and_refcount") == 0) {
                _symbol_length_and_refcount_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_body") == 0) {
                _symbol_body_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "CompiledMethod") == 0 || strcmp(type, "nmethod") == 0) {
            if (strcmp(field, "_method") == 0) {
                _nmethod_method_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_verified_entry_point") == 0) {
                _nmethod_entry_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_state") == 0) {
                _nmethod_state_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_comp_level") == 0) {
                _nmethod_level_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "Method") == 0) {
            if (strcmp(field, "_constMethod") == 0) {
                _method_constmethod_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_code") == 0) {
                _method_code_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "ConstMethod") == 0) {
            if (strcmp(field, "_constants") == 0) {
                _constmethod_constants_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_method_idnum") == 0) {
                _constmethod_idnum_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "ConstantPool") == 0) {
            if (strcmp(field, "_pool_holder") == 0) {
                _pool_holder_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "InstanceKlass") == 0) {
            if (strcmp(field, "_class_loader_data") == 0) {
                _class_loader_data_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_methods") == 0) {
                _methods_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_methods_jmethod_ids") == 0) {
                _jmethod_ids_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "ClassLoaderData") == 0) {
            if (strcmp(field, "_next") == 0) {
                _class_loader_data_next_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "java_lang_Class") == 0) {
            if (strcmp(field, "_klass_offset") == 0) {
                _klass_offset_addr = *(int**)(entry + address_offset);
            }
        } else if (strcmp(type, "JavaThread") == 0) {
            if (strcmp(field, "_osthread") == 0) {
                _thread_osthread_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_anchor") == 0) {
                _thread_anchor_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_thread_state") == 0) {
                _thread_state_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "OSThread") == 0) {
            if (strcmp(field, "_thread_id") == 0) {
                _osthread_id_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "JavaFrameAnchor") == 0) {
            if (strcmp(field, "_last_Java_sp") == 0) {
                _anchor_sp_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_last_Java_pc") == 0) {
                _anchor_pc_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "CodeBlob") == 0) {
            if (strcmp(field, "_frame_size") == 0) {
                _frame_size_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_frame_complete_offset") == 0) {
                _frame_complete_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_name") == 0) {
                _nmethod_name_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "CodeCache") == 0) {
            if (strcmp(field, "_heap") == 0) {
                _code_heap_addr = *(char***)(entry + address_offset);
            } else if (strcmp(field, "_heaps") == 0) {
                _code_heap_addr = *(char***)(entry + address_offset);
            } else if (strcmp(field, "_low_bound") == 0) {
                _code_heap_low_addr = *(const void***)(entry + address_offset);
            } else if (strcmp(field, "_high_bound") == 0) {
                _code_heap_high_addr = *(const void***)(entry + address_offset);
            }
        } else if (strcmp(type, "CodeHeap") == 0) {
            if (strcmp(field, "_memory") == 0) {
                _code_heap_memory_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_segmap") == 0) {
                _code_heap_segmap_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_log2_segment_size") == 0) {
                _code_heap_segment_shift = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "VirtualSpace") == 0) {
            if (strcmp(field, "_low_boundary") == 0) {
                _vs_low_bound_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_high_boundary") == 0) {
                _vs_high_bound_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_low") == 0) {
                _vs_low_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_high") == 0) {
                _vs_high_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "GrowableArray<int>") == 0) {
            if (strcmp(field, "_data") == 0) {
                _array_data_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "JVMFlag") == 0 || strcmp(type, "Flag") == 0) {
            if (strcmp(field, "_name") == 0 || strcmp(field, "name") == 0) {
                _flag_name_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_addr") == 0 || strcmp(field, "addr") == 0) {
                _flag_addr_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "flags") == 0) {
                _flags_addr = **(char***)(entry + address_offset);
            } else if (strcmp(field, "numFlags") == 0) {
                _flag_count = **(int**)(entry + address_offset);
            }
        } else if (strcmp(type, "PermGen") == 0) {
            _has_perm_gen = true;
        }
    }

    entry = readSymbol("gHotSpotVMTypes");
    stride = readSymbol("gHotSpotVMTypeEntryArrayStride");
    type_offset = readSymbol("gHotSpotVMTypeEntryTypeNameOffset");
    uintptr_t size_offset = readSymbol("gHotSpotVMTypeEntrySizeOffset");

    if (entry == 0 || stride == 0) {
        return;
    }

    for (;; entry += stride) {
        const char* type = *(const char**)(entry + type_offset);
        if (type == NULL) {
            break;
        }

        if (strcmp(type, "JVMFlag") == 0 || strcmp(type, "Flag") == 0) {
            _flag_size = *(int*)(entry + size_offset);
            break;
        }
    }
}

void VMStructs::resolveOffsets() {
    if (_klass_offset_addr != NULL) {
        _klass = (jfieldID)(uintptr_t)(*_klass_offset_addr << 2 | 2);
    }

    _has_class_names = _klass_name_offset >= 0
            && (_symbol_length_offset >= 0 || _symbol_length_and_refcount_offset >= 0)
            && _symbol_body_offset >= 0
            && _klass != NULL;

    _has_method_structs = _jmethod_ids_offset >= 0
            && _nmethod_method_offset >= 0
            && _nmethod_entry_offset >= 0
            && _nmethod_state_offset >= 0
            && _method_constmethod_offset >= 0
            && _method_code_offset >= 0
            && _constmethod_constants_offset >= 0
            && _constmethod_idnum_offset >= 0
            && _pool_holder_offset >= 0;

    _has_class_loader_data = _class_loader_data_offset >= 0
        && _class_loader_data_next_offset == sizeof(uintptr_t) * 8 + 8
        && _methods_offset >= 0
        && _klass != NULL
        && _lock_func != NULL && _unlock_func != NULL;

    if (_code_heap_addr != NULL && _code_heap_low_addr != NULL && _code_heap_high_addr != NULL) {
        char* code_heaps = *_code_heap_addr;
        unsigned int code_heap_count = *(unsigned int*)code_heaps;
        if (code_heap_count <= 3 && _array_data_offset >= 0) {
            char* code_heap_array = *(char**)(code_heaps + _array_data_offset);
            memcpy(_code_heap, code_heap_array, code_heap_count * sizeof(_code_heap[0]));
        }
        _code_heap_low = *_code_heap_low_addr;
        _code_heap_high = *_code_heap_high_addr;
    } else if (_code_heap_addr != NULL && _code_heap_memory_offset >= 0) {
        _code_heap[0] = *_code_heap_addr;
        _code_heap_low = *(const void**)(_code_heap[0] + _code_heap_memory_offset + _vs_low_bound_offset);
        _code_heap_high = *(const void**)(_code_heap[0] + _code_heap_memory_offset + _vs_high_bound_offset);
    }

    // Invariant: _code_heap[i] != NULL iff all CodeHeap structures are available
    if (_code_heap[0] != NULL && _code_heap_segment_shift >= 0) {
        _code_heap_segment_shift = *(int*)(_code_heap[0] + _code_heap_segment_shift);
    }
    if (_code_heap_memory_offset < 0 || _code_heap_segmap_offset < 0 ||
        _code_heap_segment_shift < 0 || _code_heap_segment_shift > 16) {
        memset(_code_heap, 0, sizeof(_code_heap));
    }
}

void VMStructs::initJvmFunctions() {
    if (!VM::isOpenJ9() && !VM::isZing()) {
        _get_stack_trace = (GetStackTraceFunc)_libjvm->findSymbolByPrefix("_ZN8JvmtiEnv13GetStackTraceEP10JavaThreadiiP");
    }

    if (VM::hotspot_version() == 8) {
        _lock_func = (LockFunc)_libjvm->findSymbol("_ZN7Monitor28lock_without_safepoint_checkEv");
        _unlock_func = (LockFunc)_libjvm->findSymbol("_ZN7Monitor6unlockEv");
    }
}

void VMStructs::initTLS(void* vm_thread) {
    for (int i = 0; i < 1024; i++) {
        if (pthread_getspecific((pthread_key_t)i) == vm_thread) {
            _tls_index = i;
            break;
        }
    }
}

void VMStructs::initThreadBridge(JNIEnv* env) {
    jthread thread;
    if (VM::jvmti()->GetCurrentThread(&thread) != 0) {
        return;
    }

    // Get eetop field - a bridge from Java Thread to VMThread
    jclass thread_class = env->GetObjectClass(thread);
    if ((_tid = env->GetFieldID(thread_class, "tid", "J")) == NULL ||
        (_eetop = env->GetFieldID(thread_class, "eetop", "J")) == NULL) {
        // No such field - probably not a HotSpot JVM
        env->ExceptionClear();

        void* j9thread = J9Ext::j9thread_self();
        if (j9thread != NULL) {
            initTLS(j9thread);
        }
    } else {
        // HotSpot
        VMThread* vm_thread = VMThread::fromJavaThread(env, thread);
        if (vm_thread != NULL) {
            _env_offset = (intptr_t)env - (intptr_t)vm_thread;
            _has_native_thread_id = _thread_osthread_offset >= 0 && _osthread_id_offset >= 0;
            initTLS(vm_thread);
        }
    }
}

void VMStructs::initLogging(JNIEnv* env) {
    // Workaround for JDK-8238460
    if (VM::hotspot_version() >= 15) {
        VMManagement* management = VM::management();
        if (management != NULL) {
            jstring log_config = management->ExecuteDiagnosticCommand(env, env->NewStringUTF("VM.log list"));
            if (log_config != NULL) {
                char cmd[128] = "VM.log what=jni+resolve=error decorators=";
                const char* s = env->GetStringUTFChars(log_config, NULL);
                if (s != NULL) {
                    const char* p = strstr(s, "#0: ");
                    if (p != NULL && (p = strchr(p + 4, ' ')) != NULL && (p = strchr(p + 1, ' ')) != NULL) {
                        const char* q = p + 1;  // start of decorators
                        while (*q > ' ') q++;
                        if (q - p < sizeof(cmd) - 41) {
                            memcpy(cmd + 41, p + 1, q - p - 1);
                        }
                    }
                    env->ReleaseStringUTFChars(log_config, s);
                }
                management->ExecuteDiagnosticCommand(env, env->NewStringUTF(cmd));
            }
        }
        env->ExceptionClear();
    }
}

VMThread* VMThread::current() {
    return (VMThread*)pthread_getspecific((pthread_key_t)_tls_index);
}

int VMThread::nativeThreadId(JNIEnv* jni, jthread thread) {
    if (_has_native_thread_id) {
        VMThread* vm_thread = fromJavaThread(jni, thread);
        return vm_thread != NULL ? vm_thread->osThreadId() : -1;
    }
    return J9Ext::GetOSThreadID(thread);
}

jmethodID ConstMethod::id() {
    const char* cpool = *(const char**) at(_constmethod_constants_offset);
    unsigned short num = *(unsigned short*) at(_constmethod_idnum_offset);
    if (cpool != NULL) {
        VMKlass* holder = *(VMKlass**)(cpool + _pool_holder_offset);
        if (holder != NULL) {
            jmethodID* ids = holder->jmethodIDs();
            if (ids != NULL && num < (size_t)ids[0]) {
                return ids[num + 1];
            }
        }
    }
    return NULL;
}

NMethod* CodeHeap::findNMethod(char* heap, const void* pc) {
    unsigned char* heap_start = *(unsigned char**)(heap + _code_heap_memory_offset + _vs_low_offset);
    unsigned char* segmap = *(unsigned char**)(heap + _code_heap_segmap_offset + _vs_low_offset);
    size_t idx = ((unsigned char*)pc - heap_start) >> _code_heap_segment_shift;

    if (segmap[idx] == 0xff) {
        return NULL;
    }
    while (segmap[idx] > 0) {
        idx -= segmap[idx];
    }

    unsigned char* block = heap_start + (idx << _code_heap_segment_shift);
    return block[sizeof(size_t)] ? (NMethod*)(block + 2 * sizeof(size_t)) : NULL;
}

void* JVMFlag::find(const char* name) {
    if (_flags_addr != NULL && _flag_size > 0) {
        for (int i = 0; i < _flag_count; i++) {
            JVMFlag* f = (JVMFlag*)(_flags_addr + i * _flag_size);
            if (f->name() != NULL && strcmp(f->name(), name) == 0) {
                return f->addr();
            }
        }
    }
    return NULL;
}
