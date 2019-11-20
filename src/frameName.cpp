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
#include "arguments.h"
#include "vmStructs.h"


FrameName::FrameName(int style, Mutex& thread_names_lock, ThreadMap& thread_names) :
    _cache(),
    _style(style),
    _thread_names_lock(thread_names_lock),
    _thread_names(thread_names)
{
    // Require printf to use standard C format regardless of system locale
    _saved_locale = uselocale(newlocale(LC_NUMERIC_MASK, "C", (locale_t)0));
    memset(_buf, 0, sizeof(_buf));
}

FrameName::~FrameName() {
    freelocale(uselocale(_saved_locale));
}

char* FrameName::truncate(char* name, int max_length) {
    if (strlen(name) > max_length && max_length >= 4) {
        strcpy(name + max_length - 4, "...)");
    }
    return name;
}

const char* FrameName::cppDemangle(const char* name) {
    if (name != NULL && name[0] == '_' && name[1] == 'Z') {
        int status;
        char* demangled = abi::__cxa_demangle(name, NULL, NULL, &status);
        if (demangled != NULL) {
            strncpy(_buf, demangled, sizeof(_buf) - 1);
            free(demangled);
            return _buf;
        }
    }
    return name;
}

char* FrameName::javaMethodName(jmethodID method, char type) {
    jclass method_class;
    char* class_name = NULL;
    char* method_name = NULL;
    char* method_sig = NULL;
    char* result;

    jvmtiEnv* jvmti = VM::jvmti();
    jvmtiError err;

    char ext[5];
    ext[0] = '_';
    ext[1] = '[';
    ext[2] = type;
    ext[3] = ']';
    ext[4] = 0;

    if ((err = jvmti->GetMethodName(method, &method_name, &method_sig, NULL)) == 0 &&
        (err = jvmti->GetMethodDeclaringClass(method, &method_class)) == 0 &&
        (err = jvmti->GetClassSignature(method_class, &class_name, NULL)) == 0) {
        // Trim 'L' and ';' off the class descriptor like 'Ljava/lang/Object;'
        result = javaClassName(class_name + 1, strlen(class_name) - 2, _style);
        strcat(result, ".");
        strcat(result, method_name);
        if (_style & STYLE_SIGNATURES) strcat(result, truncate(method_sig, 255));
        if (_style & STYLE_ANNOTATE) strcat(result, ext);
    } else {
        snprintf(_buf, sizeof(_buf) - 1, "[jvmtiError %d]", err);
        result = _buf;
    }

    jvmti->Deallocate((unsigned char*)class_name);
    jvmti->Deallocate((unsigned char*)method_sig);
    jvmti->Deallocate((unsigned char*)method_name);

    return result;
}

char* FrameName::javaClassName(const char* symbol, int length, int style) {
    char* result = _buf;

    int array_dimension = 0;
    while (*symbol == '[') {
        array_dimension++;
        symbol++;
    }

    if (array_dimension == 0) {
        strncpy(result, symbol, length);
        result[length] = 0;
    } else {
        switch (*symbol) {
            case 'B': strcpy(result, "byte");    break;
            case 'C': strcpy(result, "char");    break;
            case 'I': strcpy(result, "int");     break;
            case 'J': strcpy(result, "long");    break;
            case 'S': strcpy(result, "short");   break;
            case 'Z': strcpy(result, "boolean"); break;
            case 'F': strcpy(result, "float");   break;
            case 'D': strcpy(result, "double");  break;
            default:
                length -= array_dimension + 2;
                strncpy(result, symbol + 1, length);
                result[length] = 0;
        }

        do {
            strcat(result, "[]");
        } while (--array_dimension > 0);
    }

    if (style & STYLE_SIMPLE) {
        for (char* s = result; *s; s++) {
            if (*s == '/') result = s + 1;
        }
    }

    if (style & STYLE_DOTTED) {
        for (char* s = result; *s; s++) {
            if (*s == '/') *s = '.';
        }
    }

    return result;
}

const char* FrameName::name(ASGCT_CallFrame& frame) {
    if (frame.method_id == NULL) {
        return "[unknown]";
    }

    switch (frame.bci) {
        case BCI_KERNEL_FRAME:
        case BCI_NATIVE_FRAME: {
             const char* symbol_name = cppDemangle((const char*)frame.method_id);
             return symbol_name;
         }

        case BCI_SYMBOL: {
            VMSymbol* symbol = (VMSymbol*)frame.method_id;
            char* class_name = javaClassName(symbol->body(), symbol->length(), _style | STYLE_DOTTED);
            return class_name;
        }

        case BCI_SYMBOL_OUTSIDE_TLAB: {
            VMSymbol* symbol = (VMSymbol*)((uintptr_t)frame.method_id ^ 1);
            char* class_name = javaClassName(symbol->body(), symbol->length(), _style | STYLE_DOTTED);
            return strcat(class_name, _style & STYLE_DOTTED ? " (out)" : "");
        }

        case BCI_THREAD_ID: {
            int tid = (int)(uintptr_t)frame.method_id;
            MutexLocker ml(_thread_names_lock);
            ThreadMap::iterator it = _thread_names.find(tid);
            if (it != _thread_names.end()) {
                snprintf(_buf, sizeof(_buf) - 1, "[%s tid=%d]", it->second.c_str(), tid);
            } else {
                snprintf(_buf, sizeof(_buf) - 1, "[tid=%d]", tid);
            }
            return _buf;
        }

        case BCI_ERROR: {
            snprintf(_buf, sizeof(_buf) - 1, "[%s]", (const char*)frame.method_id);
            return _buf;
        }

        default: {
            JMethodCache::iterator it = _cache.lower_bound(frame.method_id);
            if (it != _cache.end() && it->first == frame.method_id) {
                return it->second.c_str();
            }

            char type = FRAME_TYPE_UNKNOWN_JAVA;
            if (frame.bci >= (BCI_OFFSET_COMP +  BCI_SMALLEST_USED_BY_VM)) {
              type = FRAME_TYPE_COMPILED_JAVA;
            }
            if (frame.bci >= (BCI_OFFSET_INTERP +  BCI_SMALLEST_USED_BY_VM)) {
              type = FRAME_TYPE_INTERPRETED_JAVA;
            }
            if (frame.bci >= (BCI_OFFSET_INLINED +  BCI_SMALLEST_USED_BY_VM)) {
              type = FRAME_TYPE_INLINED_JAVA;
            }

           const char* newName = javaMethodName(frame.method_id, type);
            _cache.insert(it, JMethodCache::value_type(frame.method_id, newName));
            return newName;
        }
    }
}
