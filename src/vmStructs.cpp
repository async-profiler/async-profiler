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


int VMStructs::_klass_name_offset = -1;
int VMStructs::_symbol_length_offset = -1;
int VMStructs::_symbol_length_and_refcount_offset = -1;
int VMStructs::_symbol_body_offset = -1;
int VMStructs::_class_klass_offset = -1;
int VMStructs::_thread_osthread_offset = -1;
int VMStructs::_osthread_id_offset = -1;
bool VMStructs::_has_perm_gen = false;
jfieldID VMStructs::_eetop = NULL;

static uintptr_t readSymbol(NativeLib* lib, const char* symbol_name) {
    const void* symbol = lib->findSymbol(symbol_name);
    if (symbol == NULL) {
        // Avoid JVM crash in case of missing symbols
        return 0;
    }
    return *(uintptr_t*)symbol;
}

void VMStructs::init(NativeLib* libjvm) {
    if (available()) {
        return;
    }

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
            }
        } else if (strcmp(type, "OSThread") == 0) {
            if (strcmp(field, "_thread_id") == 0) {
                _osthread_id_offset = *(int*)(entry + offset_offset);
            }
        } else if (strcmp(type, "PermGen") == 0) {
            _has_perm_gen = true;
        }

        entry += stride;
    }

    // Get eetop field - a bridge from Java Thread to VMThread
    if (_thread_osthread_offset >= 0 && _osthread_id_offset >= 0) {
        JNIEnv* env = VM::jni();
        jclass threadClass = env->FindClass("java/lang/Thread");
        if (threadClass != NULL) {
            _eetop = env->GetFieldID(threadClass, "eetop", "J");
        }
    }
}
