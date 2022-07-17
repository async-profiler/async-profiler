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
#include "profiler.h"
#include "vmStructs.h"


static inline bool isDigit(char c) {
    return c >= '0' && c <= '9';
}


Matcher::Matcher(const char* pattern) {
    if (pattern[0] == '*') {
        _type = MATCH_ENDS_WITH;
        _pattern = strdup(pattern + 1);
    } else {
        _type = MATCH_EQUALS;
        _pattern = strdup(pattern);
    }

    _len = strlen(_pattern);
    if (_len > 0 && _pattern[_len - 1] == '*') {
        _type = _type == MATCH_EQUALS ? MATCH_STARTS_WITH : MATCH_CONTAINS;
        _pattern[--_len] = 0;
    }
}

Matcher::~Matcher() {
    free(_pattern);
}

Matcher::Matcher(const Matcher& m) {
    _type = m._type;
    _pattern = strdup(m._pattern);
    _len = m._len;
}

Matcher& Matcher::operator=(const Matcher& m) {
    free(_pattern);

    _type = m._type;
    _pattern = strdup(m._pattern);
    _len = m._len;

    return *this;
}

bool Matcher::matches(const char* s) {
    switch (_type) {
        case MATCH_EQUALS:
            return strcmp(s, _pattern) == 0;
        case MATCH_CONTAINS:
            return strstr(s, _pattern) != NULL;
        case MATCH_STARTS_WITH:
            return strncmp(s, _pattern, _len) == 0;
        case MATCH_ENDS_WITH:
            int slen = strlen(s);
            return slen >= _len && strcmp(s + slen - _len, _pattern) == 0;
    }
    return false;
}


JMethodCache FrameName::_cache;

FrameName::FrameName(Arguments& args, int style, int epoch, Mutex& thread_names_lock, ThreadMap& thread_names) :
    _class_names(),
    _include(),
    _exclude(),
    _style(style),
    _cache_epoch((unsigned char)epoch),
    _cache_max_age(args._mcache),
    _thread_names_lock(thread_names_lock),
    _thread_names(thread_names)
{
    // Require printf to use standard C format regardless of system locale
    _saved_locale = uselocale(newlocale(LC_NUMERIC_MASK, "C", (locale_t)0));
    memset(_buf, 0, sizeof(_buf));

    buildFilter(_include, args._buf, args._include);
    buildFilter(_exclude, args._buf, args._exclude);

    Profiler::instance()->classMap()->collect(_class_names);
}

FrameName::~FrameName() {
    if (_cache_max_age == 0) {
        _cache.clear();
    } else {
        // Remove stale methods from the cache, leave the fresh ones for the next profiling session
        for (JMethodCache::iterator it = _cache.begin(); it != _cache.end(); ) {
            if (_cache_epoch - (unsigned char)it->second[0] >= _cache_max_age) {
                _cache.erase(it++);
            } else {
                ++it;
            }
        }
    }

    freelocale(uselocale(_saved_locale));
}

void FrameName::buildFilter(std::vector<Matcher>& vector, const char* base, int offset) {
    while (offset != 0) {
        vector.push_back(base + offset);
        offset = ((int*)(base + offset))[-1];
    }
}

char* FrameName::truncate(char* name, int max_length) {
    if (strlen(name) > max_length && max_length >= 4) {
        strcpy(name + max_length - 4, "...)");
    }
    return name;
}

const char* FrameName::decodeNativeSymbol(const char* name) {
    const char* lib_name = (_style & STYLE_LIB_NAMES) ? Profiler::instance()->getLibraryName(name) : NULL;

    if (name[0] == '_' && name[1] == 'Z') {
        int status;
        char* demangled = abi::__cxa_demangle(name, NULL, NULL, &status);
        if (demangled != NULL) {
            if (lib_name != NULL) {
                snprintf(_buf, sizeof(_buf) - 1, "%s`%s", lib_name, demangled);
            } else {
                strncpy(_buf, demangled, sizeof(_buf) - 1);
            }
            free(demangled);
            return _buf;
        }
    }

    if (lib_name != NULL) {
        snprintf(_buf, sizeof(_buf) - 1, "%s`%s", lib_name, name);
        return _buf;
    } else {
        return name;
    }
}

const char* FrameName::typeSuffix(FrameTypeId type) {
    if (_style & STYLE_ANNOTATE) {
        switch (type) {
            case FRAME_INTERPRETED:  return "_[0]";
            case FRAME_JIT_COMPILED: return "_[j]";
            case FRAME_INLINED:      return "_[i]";
            case FRAME_C1_COMPILED:  return "_[1]";
            default:                 return NULL;
        }
    }
    return NULL;
}

char* FrameName::javaMethodName(jmethodID method) {
    jclass method_class;
    char* class_name = NULL;
    char* method_name = NULL;
    char* method_sig = NULL;
    char* result;

    jvmtiEnv* jvmti = VM::jvmti();
    jvmtiError err;

    if ((err = jvmti->GetMethodName(method, &method_name, &method_sig, NULL)) == 0 &&
        (err = jvmti->GetMethodDeclaringClass(method, &method_class)) == 0 &&
        (err = jvmti->GetClassSignature(method_class, &class_name, NULL)) == 0) {
        // Trim 'L' and ';' off the class descriptor like 'Ljava/lang/Object;'
        result = javaClassName(class_name + 1, strlen(class_name) - 2, _style);
        strcat(result, ".");
        strcat(result, method_name);
        if (_style & STYLE_SIGNATURES) strcat(result, truncate(method_sig, 255));
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
            if (*s == '/' && !isDigit(s[1])) result = s + 1;
        }
    }

    if (style & STYLE_DOTTED) {
        for (char* s = result; *s; s++) {
            if (*s == '/' && !isDigit(s[1])) *s = '.';
        }
    }

    return result;
}

const char* FrameName::name(ASGCT_CallFrame& frame, bool for_matching) {
    if (frame.method_id == NULL) {
        return "[unknown]";
    }

    switch (frame.bci) {
        case BCI_NATIVE_FRAME:
            return decodeNativeSymbol((const char*)frame.method_id);

        case BCI_ALLOC:
        case BCI_ALLOC_OUTSIDE_TLAB:
        case BCI_LOCK:
        case BCI_PARK: {
            const char* symbol = _class_names[(uintptr_t)frame.method_id];
            char* class_name = javaClassName(symbol, strlen(symbol), _style | STYLE_DOTTED);
            if (!for_matching && !(_style & STYLE_DOTTED)) {
                strcat(class_name, frame.bci == BCI_ALLOC_OUTSIDE_TLAB ? "_[k]" : "_[i]");
            }
            return class_name;
        }

        case BCI_THREAD_ID: {
            int tid = (int)(uintptr_t)frame.method_id;
            MutexLocker ml(_thread_names_lock);
            ThreadMap::iterator it = _thread_names.find(tid);
            if (for_matching) {
                return it != _thread_names.end() ? it->second.c_str() : "";
            } else if (it != _thread_names.end()) {
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
            const char* type_suffix = typeSuffix(FrameType::decode(frame.bci));

            JMethodCache::iterator it = _cache.lower_bound(frame.method_id);
            if (it != _cache.end() && it->first == frame.method_id) {
                it->second[0] = _cache_epoch;
                if (type_suffix != NULL) {
                    snprintf(_buf, sizeof(_buf) - 1, "%s%s", it->second.c_str() + 1, type_suffix);
                    return _buf;
                }
                return it->second.c_str() + 1;
            }

            char* newName = javaMethodName(frame.method_id);
            _cache.insert(it, JMethodCache::value_type(frame.method_id, std::string(1, _cache_epoch) + newName));
            return type_suffix != NULL ? strcat(newName, type_suffix) : newName;
        }
    }
}

bool FrameName::include(const char* frame_name) {
    for (int i = 0; i < _include.size(); i++) {
        if (_include[i].matches(frame_name)) {
            return true;
        }
    }
    return false;
}

bool FrameName::exclude(const char* frame_name) {
    for (int i = 0; i < _exclude.size(); i++) {
        if (_exclude[i].matches(frame_name)) {
            return true;
        }
    }
    return false;
}

