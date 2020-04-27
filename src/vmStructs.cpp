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

#include <stdint.h>
#include <string.h>
#include "vmStructs.h"
#include "vmEntry.h"


jfieldID VMStructs::_eetop;
jfieldID VMStructs::_tid;
intptr_t VMStructs::_env_offset;
int VMStructs::_klass_name_offset = -1;
int VMStructs::_symbol_length_offset = -1;
int VMStructs::_symbol_length_and_refcount_offset = -1;
int VMStructs::_symbol_body_offset = -1;
int VMStructs::_class_klass_offset = -1;
int VMStructs::_thread_osthread_offset = -1;
int VMStructs::_thread_anchor_offset = -1;
int VMStructs::_osthread_id_offset = -1;
int VMStructs::_anchor_sp_offset = -1;
int VMStructs::_anchor_pc_offset = -1;
int VMStructs::_frame_size_offset = -1;
bool VMStructs::_has_perm_gen = false;

static uintptr_t readSymbol(NativeCodeCache* lib, const char* symbol_name) {
    const void* symbol = lib->findSymbol(symbol_name);
    if (symbol == NULL) {
        // Avoid JVM crash in case of missing symbols
        return 0;
    }
    return *(uintptr_t*)symbol;
}

void VMStructs::init(NativeCodeCache* libjvm) {
    uintptr_t entry = readSymbol(libjvm, "gHotSpotVMStructs");
    uintptr_t stride = readSymbol(libjvm, "gHotSpotVMStructEntryArrayStride");
    uintptr_t type_offset = readSymbol(libjvm, "gHotSpotVMStructEntryTypeNameOffset");
    uintptr_t field_offset = readSymbol(libjvm, "gHotSpotVMStructEntryFieldNameOffset");
    uintptr_t offset_offset = readSymbol(libjvm, "gHotSpotVMStructEntryOffsetOffset");
    uintptr_t address_offset = readSymbol(libjvm, "gHotSpotVMStructEntryAddressOffset");

    if (entry == 0 || stride == 0) {
        return;
    }

    while (true) {
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
        } else if (strcmp(type, "java_lang_Class") == 0) {
            if (strcmp(field, "_klass_offset") == 0) {
                _class_klass_offset = **(int**)(entry + address_offset);
            }
        } else if (strcmp(type, "JavaThread") == 0) {
            if (strcmp(field, "_osthread") == 0) {
                _thread_osthread_offset = *(int*)(entry + offset_offset);
            } else if (strcmp(field, "_anchor") == 0) {
                _thread_anchor_offset = *(int*)(entry + offset_offset);
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
            }
        } else if (strcmp(type, "PermGen") == 0) {
            _has_perm_gen = true;
        }

        entry += stride;
    }
}

bool VMStructs::initThreadBridge() {
    // Get eetop field - a bridge from Java Thread to VMThread
    jthread thread;
    if (VM::jvmti()->GetCurrentThread(&thread) != 0) {
        return false;
    }

    JNIEnv* env = VM::jni();
    jclass thread_class = env->GetObjectClass(thread);
    _eetop = env->GetFieldID(thread_class, "eetop", "J");
    _tid = env->GetFieldID(thread_class, "tid", "J");
    if (_eetop == NULL || _tid == NULL) {
        return false;
    }

    VMThread* vm_thread = VMThread::fromJavaThread(env, thread);
    if (vm_thread == NULL) {
        return false;
    }

    _env_offset = (intptr_t)env - (intptr_t)vm_thread;
    return true;
}
