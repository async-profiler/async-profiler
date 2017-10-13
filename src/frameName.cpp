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

#include <cxxabi.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "frameName.h"


const char* FrameName::cppDemangle(const char* name) {
    if (name != NULL && name[0] == '_' && name[1] == 'Z') {
        int status;
        char* demangled = abi::__cxa_demangle(name, NULL, NULL, &status);
        if (demangled != NULL) {
            strncpy(_buf, demangled, sizeof(_buf));
            free(demangled);
            return _buf;
        }
    }
    return name;
}

char* FrameName::javaClassName(VMKlass* klass) {
    VMSymbol* symbol = klass->name();
    return javaClassName(symbol->body(), symbol->length(), true);
}

char* FrameName::javaClassName(const char* symbol, int length, bool dotted) {
    int array_dimension = 0;
    while (*symbol == '[') {
        array_dimension++;
        symbol++;
    }

    if (array_dimension == 0) {
        strncpy(_buf, symbol, length);
        _buf[length] = 0;
    } else {
        switch (*symbol) {
            case 'B': strcpy(_buf, "byte");    break;
            case 'C': strcpy(_buf, "char");    break;
            case 'I': strcpy(_buf, "int");     break;
            case 'J': strcpy(_buf, "long");    break;
            case 'S': strcpy(_buf, "short");   break;
            case 'Z': strcpy(_buf, "boolean"); break;
            case 'F': strcpy(_buf, "float");   break;
            case 'D': strcpy(_buf, "double");  break;
            default:
                length -= array_dimension + 2;
                strncpy(_buf, symbol + 1, length);
                _buf[length] = 0;
        }

        do {
            strcat(_buf, "[]");
        } while (--array_dimension > 0);
    }

    if (dotted) {
        for (char* s = _buf; *s; s++) {
            if (*s == '/') *s = '.';
        }
    }

    return _buf;
}

FrameName::FrameName(ASGCT_CallFrame& frame, bool dotted) {
    if (frame.method_id == NULL) {
        _str = "[unknown]";
    } else if (frame.bci == BCI_NATIVE_FRAME) {
        _str = cppDemangle((const char*)frame.method_id);
    } else if (frame.bci == BCI_ALLOC_NEW_TLAB) {
        VMKlass* alloc_class = (VMKlass*)frame.method_id;
        _str = strcat(javaClassName(alloc_class), "_[i]");
    } else if (frame.bci == BCI_ALLOC_OUTSIDE_TLAB) {
        VMKlass* alloc_class = (VMKlass*)((uintptr_t)frame.method_id ^ 1);
        _str = strcat(javaClassName(alloc_class), "_[k]");
    } else {
        jclass method_class;
        char* class_name = NULL;
        char* method_name = NULL;

        jvmtiEnv* jvmti = VM::jvmti();
        jvmtiError err;

        if ((err = jvmti->GetMethodName(frame.method_id, &method_name, NULL, NULL)) == 0 &&
            (err = jvmti->GetMethodDeclaringClass(frame.method_id, &method_class)) == 0 &&
            (err = jvmti->GetClassSignature(method_class, &class_name, NULL)) == 0) {
            // Trim 'L' and ';' off the class descriptor like 'Ljava/lang/Object;'
            char* s = javaClassName(class_name + 1, strlen(class_name) - 2, dotted);
            strcat(s, ".");
            strcat(s, method_name);
            _str = s;
        } else {
            snprintf(_buf, sizeof(_buf), "[jvmtiError %d]", err);
            _str = _buf;
        }

        jvmti->Deallocate((unsigned char*)class_name);
        jvmti->Deallocate((unsigned char*)method_name);
    }
}
