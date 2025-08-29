/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "demangle.h"
#include "dictionary.h"
#include "index.h"
#include "lookup.h"
#include "profiler.h"
#include "vmStructs.h"

jint MethodInfo::getLineNumber(jint bci) {
    if (_line_number_table_size == 0) {
        return 0;
    }

    int i = 1;
    while (i < _line_number_table_size && bci >= _line_number_table[i].start_location) {
        i++;
    }
    return _line_number_table[i - 1].line_number;
}

MethodMap::~MethodMap() {
    jvmtiEnv* jvmti = VM::jvmti();
    for (const_iterator it = begin(); it != end(); ++it) {
        jvmtiLineNumberEntry* line_number_table = it->second._line_number_table;
        if (line_number_table != NULL) {
            jvmti->Deallocate((unsigned char*)line_number_table);
        }
    }
}

size_t MethodMap::usedMemory() {
    size_t bytes = 0;
    for (const_iterator it = begin(); it != end(); ++it) {
        bytes += sizeof(jmethodID) + sizeof(MethodInfo);
        bytes += it->second._line_number_table_size * sizeof(jvmtiLineNumberEntry);
    }
    return bytes;
}

MethodInfo* Lookup::resolveMethod(ASGCT_CallFrame& frame) {
    jmethodID method = frame.method_id;
    MethodInfo* mi = &(*_method_map)[method];

    bool first_time = mi->_key == 0;
    if (first_time) {
        mi->_key = _method_map->size();
    }

    if (!mi->_mark) {
        mi->_mark = true;
        if (method == NULL) {
            fillNativeMethodInfo(mi, "unknown", NULL);
        } else if (frame.bci > BCI_NATIVE_FRAME) {
            if (!fillJavaMethodInfo(mi, method, first_time)) {
                fillNativeMethodInfo(mi, "stale_jmethodID", NULL);
            }
        } else if (frame.bci == BCI_NATIVE_FRAME) {
            const char* name = (const char*)method;
            fillNativeMethodInfo(mi, name, Profiler::instance()->getLibraryName(name));
        } else if (frame.bci == BCI_ADDRESS) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%p", method);
            fillNativeMethodInfo(mi, buf, NULL);
        } else if (frame.bci == BCI_ERROR) {
            fillNativeMethodInfo(mi, (const char*)method, NULL);
        } else if (frame.bci == BCI_CPU) {
            char buf[32];
            snprintf(buf, sizeof(buf), "CPU-%d", ((int)(uintptr_t)method) & 0x7fff);
            fillNativeMethodInfo(mi, buf, NULL);
        } else {
            fillJavaClassInfo(mi, (uintptr_t)method);
        }
    }

    return mi;
}

u32 Lookup::getPackage(const char* class_name) {
    assert(_packages != nullptr);
    const char* package = strrchr(class_name, '/');
    if (package == NULL) {
        return 0;
    }
    if (package[1] >= '0' && package[1] <= '9') {
        // Seems like a hidden or anonymous class, e.g. com/example/Foo/0x012345
        do {
            if (package == class_name) return 0;
        } while (*--package != '/');
    }
    if (class_name[0] == '[') {
        class_name = strchr(class_name, 'L') + 1;
    }
    return _packages->indexOf(class_name, package - class_name);
}

void Lookup::fillNativeMethodInfo(MethodInfo* mi, const char* name, const char* lib_name) {
    if (lib_name == NULL) {
        mi->_class = _classes->lookup("");
    } else if (lib_name[0] == '[' && lib_name[1] != 0) {
        mi->_class = _classes->lookup(lib_name + 1, strlen(lib_name) - 2);
    } else {
        mi->_class = _classes->lookup(lib_name);
    }

    mi->_modifiers = 0x100;

    if (Demangle::needsDemangling(name)) {
        char* demangled = Demangle::demangle(name, false);
        if (demangled != NULL) {
            mi->_name = _symbols->indexOf(demangled);
            mi->_sig = _symbols->indexOf("()L;");
            mi->_type = FRAME_CPP;
            free(demangled);
            return;
        }
    }

    size_t len = strlen(name);
    if (len >= 4 && strcmp(name + len - 4, "_[k]") == 0) {
        mi->_name = _symbols->indexOf(name, len - 4);
        mi->_sig = _symbols->indexOf("(Lk;)L;");
        mi->_type = FRAME_KERNEL;
    } else {
        mi->_name = _symbols->indexOf(name);
        mi->_sig = _symbols->indexOf("()L;");
        mi->_type = FRAME_NATIVE;
    }
}

bool Lookup::fillJavaMethodInfo(MethodInfo* mi, jmethodID method, bool first_time) {
    if (VMMethod::isStaleMethodId(method)) {
        return false;
    }

    jclass method_class = NULL;
    char* class_name = NULL;
    char* method_name = NULL;
    char* method_sig = NULL;

    jvmtiEnv* jvmti = VM::jvmti();
    jvmtiError err;

    if ((err = jvmti->GetMethodName(method, &method_name, &method_sig, NULL)) == 0 &&
        (err = jvmti->GetMethodDeclaringClass(method, &method_class)) == 0) {
        mi->_sig = _symbols->indexOf(method_sig);
        mi->_name = _symbols->indexOf(method_name);

        if ((err = jvmti->GetClassSignature(method_class, &class_name, NULL)) == 0) {
            mi->_class = _classes->lookup(class_name + 1, strlen(class_name) - 2);
        }
    }

    if (method_class) {
        _jni->DeleteLocalRef(method_class);
    }
    jvmti->Deallocate((unsigned char*)method_sig);
    jvmti->Deallocate((unsigned char*)method_name);
    jvmti->Deallocate((unsigned char*)class_name);

    if (err != 0) {
        return false;
    }

    if (first_time && jvmti->GetMethodModifiers(method, &mi->_modifiers) != 0) {
        mi->_modifiers = 0;
    }

    if (first_time && jvmti->GetLineNumberTable(method, &mi->_line_number_table_size, &mi->_line_number_table) != 0) {
        mi->_line_number_table_size = 0;
        mi->_line_number_table = NULL;
    }

    mi->_type = FRAME_INTERPRETED;
    return true;
}

void Lookup::fillJavaClassInfo(MethodInfo* mi, u32 class_id) {
    mi->_class = class_id;
    mi->_name = _symbols->indexOf("");
    mi->_sig = _symbols->indexOf("()L;");
    mi->_type = FRAME_INLINED;
}
