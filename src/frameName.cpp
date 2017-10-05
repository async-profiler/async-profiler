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

const char* FrameName::javaClassName(const char* symbol, int length, bool dotted) {
    const char* s = symbol;
    while (*s == '[') {
        s++;
    }

    int array_dimension = s - symbol;
    if (array_dimension != 0) {
        switch (*s) {
            case 'B': strcpy(_buf, "byte");    break;
            case 'C': strcpy(_buf, "char");    break;
            case 'I': strcpy(_buf, "int");     break;
            case 'J': strcpy(_buf, "long");    break;
            case 'S': strcpy(_buf, "short");   break;
            case 'Z': strcpy(_buf, "boolean"); break;
            case 'F': strcpy(_buf, "float");   break;
            case 'D': strcpy(_buf, "double");  break;
            case 'L': strncpy(_buf, s + 1, length -= array_dimension -= 2); _buf[break;
        }
    }

    while (array_dimension-- > 0) {
        strcat(_buf, "[]");
    }

    if (dotted) {
        for (char* s = name; *s; s++) {
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
        } else if (frame.bci == BCI_ALLOC_NEW || frame.bci == BCI_ALLOC_OUT) {
            VMSymbol* symbol = (VMSymbol*)frame.method_id;
            _str = javaClassName(symbol->body(), symbol->length());
        } else {
            jclass method_class;
            char* class_name = NULL;
            char* method_name = NULL;

            jvmtiEnv* jvmti = VM::jvmti();
            jvmtiError err;

            if ((err = jvmti->GetMethodName(frame.method_id, &method_name, NULL, NULL)) == 0 &&
                (err = jvmti->GetMethodDeclaringClass(frame.method_id, &method_class)) == 0 &&
                (err = jvmti->GetClassSignature(method_class, &class_name, NULL)) == 0) {
                snprintf(_buf, sizeof(_buf), "%s.%s", fixClassName(class_name, dotted), method_name);
            } else {
                snprintf(_buf, sizeof(_buf), "[jvmtiError %d]", err);
            }
            _str = _buf;

            jvmti->Deallocate((unsigned char*)class_name);
            jvmti->Deallocate((unsigned char*)method_name);
        }
    }

    const char* toString() {
        return _str;
    }
};
