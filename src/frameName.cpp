/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "demangle.h"
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
    _str(),
    _style(style),
    _cache_epoch((unsigned char)epoch),
    _cache_max_age(args._mcache),
    _thread_names_lock(thread_names_lock),
    _thread_names(thread_names),
    _jni(VM::jni())
{
    // Require printf to use standard C format regardless of system locale
    _saved_locale = uselocale(newlocale(LC_NUMERIC_MASK, "C", (locale_t)0));

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

const char* FrameName::decodeNativeSymbol(const char* name) {
    const char* lib_name = (_style & STYLE_LIB_NAMES) ? Profiler::instance()->getLibraryName(name) : NULL;

    if (name[0] == '_' && name[1] == 'Z') {
        char* demangled = Demangle::demangle(name, _style & STYLE_SIGNATURES);
        if (demangled != NULL) {
            if (lib_name != NULL) {
                _str.assign(lib_name).append("`").append(demangled);
            } else {
                _str.assign(demangled);
            }
            free(demangled);
            return _str.c_str();
        }
    }

    if (lib_name != NULL) {
        return _str.assign(lib_name).append("`").append(name).c_str();
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

void FrameName::javaMethodName(jmethodID method) {
    if (VMStructs::hasMethodStructs()) {
        // Workaround for JDK-8313816
        VMMethod* vm_method = VMMethod::fromMethodID(method);
        if (vm_method == NULL || vm_method->id() == NULL) {
            _str.assign("[stale_jmethodID]");
            return;
        }
    }

    jclass method_class;
    char* class_name = NULL;
    char* method_name = NULL;
    char* method_sig = NULL;

    jvmtiEnv* jvmti = VM::jvmti();
    jvmtiError err;

    if ((err = jvmti->GetMethodName(method, &method_name, &method_sig, NULL)) == 0 &&
        (err = jvmti->GetMethodDeclaringClass(method, &method_class)) == 0 &&
        (err = jvmti->GetClassSignature(method_class, &class_name, NULL)) == 0) {
        // Trim 'L' and ';' off the class descriptor like 'Ljava/lang/Object;'
        javaClassName(class_name + 1, strlen(class_name) - 2, _style);
        _str.append(".").append(method_name);
        if (_style & STYLE_SIGNATURES) {
            if (_style & STYLE_NO_SEMICOLON) {
                for (char* s = method_sig; *s; s++) {
                    if (*s == ';') *s = '|';
                }
            }
            _str.append(method_sig);
        }
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "[jvmtiError %d]", err);
        _str.assign(buf);
    }

    if (method_class) {
        _jni->DeleteLocalRef(method_class);
    }
    jvmti->Deallocate((unsigned char*)class_name);
    jvmti->Deallocate((unsigned char*)method_sig);
    jvmti->Deallocate((unsigned char*)method_name);
}

void FrameName::javaClassName(const char* symbol, size_t length, int style) {
    int array_dimension = 0;
    while (*symbol == '[') {
        array_dimension++;
        symbol++;
    }

    if (array_dimension == 0) {
        _str.assign(symbol, length);
    } else {
        switch (*symbol) {
            case 'B': _str.assign("byte");    break;
            case 'C': _str.assign("char");    break;
            case 'I': _str.assign("int");     break;
            case 'J': _str.assign("long");    break;
            case 'S': _str.assign("short");   break;
            case 'Z': _str.assign("boolean"); break;
            case 'F': _str.assign("float");   break;
            case 'D': _str.assign("double");  break;
            default:  _str.assign(symbol + 1, length - array_dimension - 2);
        }

        do {
            _str += "[]";
        } while (--array_dimension > 0);
    }

    if (style & STYLE_NORMALIZE) {
        size_t size = _str.size();
        for (ssize_t i = size - 2; i > 0; i--) {
            if (_str[i] == '/' || _str[i] == '.') {
                if (isDigit(_str[i + 1])) {
                    _str.resize(i);
                }
                break;
            }
        }
    }

    if (style & STYLE_SIMPLE) {
        size_t start = 0;
        size_t size = _str.size();
        for (size_t i = 0; i < size; i++) {
            if (_str[i] == '/' && !isDigit(_str[i + 1])) start = i + 1;
        }
        _str.erase(0, start);
    }

    if (style & STYLE_DOTTED) {
        size_t size = _str.size();
        for (size_t i = 0; i < size; i++) {
            if (_str[i] == '/' && !isDigit(_str[i + 1])) _str[i] = '.';
        }
    }
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
            javaClassName(symbol, strlen(symbol), _style | STYLE_DOTTED);
            if (!for_matching && !(_style & STYLE_DOTTED)) {
                _str += frame.bci == BCI_ALLOC_OUTSIDE_TLAB ? "_[k]" : "_[i]";
            }
            return _str.c_str();
        }

        case BCI_THREAD_ID: {
            int tid = (int)(uintptr_t)frame.method_id;
            MutexLocker ml(_thread_names_lock);
            ThreadMap::iterator it = _thread_names.find(tid);
            if (for_matching) {
                return it != _thread_names.end() ? it->second.c_str() : "";
            }

            char buf[32];
            snprintf(buf, sizeof(buf), "tid=%d]", tid);
            if (it != _thread_names.end()) {
                return _str.assign("[").append(it->second).append(" ").append(buf).c_str();
            } else {
                return _str.assign("[").append(buf).c_str();
            }
        }

        case BCI_ADDRESS: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%p", frame.method_id);
            return _str.assign(buf).c_str();
        }

        case BCI_ERROR:
            return _str.assign("[").append((const char*)frame.method_id).append("]").c_str();

        default: {
            const char* type_suffix = typeSuffix(FrameType::decode(frame.bci));

            JMethodCache::iterator it = _cache.lower_bound(frame.method_id);
            if (it != _cache.end() && it->first == frame.method_id) {
                it->second[0] = _cache_epoch;
                const char* name = it->second.c_str() + 1;
                if (type_suffix != NULL) {
                    return _str.assign(name).append(type_suffix).c_str();
                }
                return name;
            }

            javaMethodName(frame.method_id);
            _cache.insert(it, JMethodCache::value_type(frame.method_id, std::string(1, _cache_epoch) + _str));
            if (type_suffix != NULL) {
                _str += type_suffix;
            }
            return _str.c_str();
        }
    }
}

FrameTypeId FrameName::type(ASGCT_CallFrame& frame) {
    if (frame.method_id == NULL) {
        return FRAME_NATIVE;
    }

    switch (frame.bci) {
        case BCI_NATIVE_FRAME: {
            const char* name = (const char*)frame.method_id;
            if ((name[0] == '_' && name[1] == 'Z') ||
                (name[0] == '+' && name[1] == '[') ||
                (name[0] == '-' && name[1] == '[')) {
                return FRAME_CPP;
            } else {
                size_t len = strlen(name);
                return len > 4 && strcmp(name + len - 4, "_[k]") == 0 ? FRAME_KERNEL : FRAME_NATIVE;
            }
        }

        case BCI_ALLOC:
        case BCI_LOCK:
        case BCI_PARK:
            return FRAME_INLINED;

        case BCI_ALLOC_OUTSIDE_TLAB:
            return FRAME_KERNEL;

        case BCI_THREAD_ID:
        case BCI_ADDRESS:
        case BCI_ERROR:
            return FRAME_NATIVE;

        default:
            return FrameType::decode(frame.bci);
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
