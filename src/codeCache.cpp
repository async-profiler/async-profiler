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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cxxabi.h>
#include "codeCache.h"
#include "vmEntry.h"


static inline bool isString(jmethodID method) {
    return (intptr_t)method < 0;
}

static inline char* asString(jmethodID method) {
    return (char*)(-(intptr_t)method);
}

static inline jmethodID asJavaMethod(const char* name) {
    return (jmethodID)(-(intptr_t)name);
}


MethodName::MethodName(jmethodID method, bool dotted) {
    if (method == NULL) {
        _str = "[unknown]";
    } else if (isString(method)) {
        _str = demangle(asString(method));
    } else {
        jclass method_class;
        char* class_name;
        char* method_name;

        jvmtiEnv* jvmti = VM::jvmti();
        jvmti->GetMethodName(method, &method_name, NULL, NULL);
        jvmti->GetMethodDeclaringClass(method, &method_class);
        jvmti->GetClassSignature(method_class, &class_name, NULL);

        snprintf(_buf, sizeof(_buf), "%s.%s", fixClassName(class_name, dotted), method_name);
        _str = _buf;

        jvmti->Deallocate((unsigned char*)class_name);
        jvmti->Deallocate((unsigned char*)method_name);
    }
}

char* MethodName::fixClassName(char* name, bool dotted) {
    if (dotted) {
        for (char* s = name + 1; *s; s++) {
            if (*s == '/') *s = '.';
        }
    }

    name[strlen(name) - 1] = 0;
    return name + 1;
}

char* MethodName::demangle(char* name) {
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


StringTable* StringTable::allocate(StringTable* next, int capacity, int size) {
    StringTable* st = (StringTable*)malloc(sizeof(StringTable) + capacity);
    st->_next = next;
    st->_capacity = capacity;
    st->_size = size;
    return st;
}

StringTable* StringTable::destroy() {
    StringTable* next = _next;
    free(this);
    return next;
}

const char* StringTable::add(const char* s, int length) {
    const char* result = (const char*)memcpy(_data + _size, s, length);
    _size += length;
    return result;
}


CodeCache::CodeCache(const char* name, const void* min_address, const void* max_address) {
    _name = strdup(name);
    _capacity = INITIAL_CODE_CACHE_CAPACITY;
    _count = 0;
    _blobs = (CodeBlob*)malloc(_capacity * sizeof(CodeBlob));
    _strings = NULL;
    _min_address = min_address;
    _max_address = max_address;
}

CodeCache::~CodeCache() {
    for (StringTable* st = _strings; st != NULL; ) {
        st = st->destroy();
    }
    free(_blobs);
    free(_name);
}

const char* CodeCache::addString(const char* s) {
    int length = strlen(s) + 1;
    if (_strings == NULL || _strings->remaining() < length) {
        _strings = StringTable::allocate(_strings, STRING_TABLE_CHUNK);
    }
    return _strings->add(s, length);
}

const char* CodeCache::addStrings(const char* s, int length) {
    _strings = StringTable::allocate(_strings, length);
    return _strings->add(s, length);
}

void CodeCache::add(const void* start, int length, jmethodID method) {
    const char* end = (const char*)start + length;
    if (start < _min_address) _min_address = start;
    if (end > _max_address) _max_address = end;

    if (_count >= _capacity) {
        // Note: we do not free old block here; it can be concurrently used in signal handler
        CodeBlob* new_blobs = (CodeBlob*)malloc(_capacity * sizeof(CodeBlob) * 2);
        memcpy(new_blobs, _blobs, _capacity * sizeof(CodeBlob));
        _capacity *= 2;
        _blobs = new_blobs;
        __sync_synchronize();
    }

    _blobs[_count]._start = start;
    _blobs[_count]._end = end;
    _blobs[_count]._method = method;
    __sync_synchronize();
    _count++;

}

void CodeCache::add(const void* start, int length, const char* name) {
    add(start, length, asJavaMethod(name));
}

void CodeCache::remove(const void* start, jmethodID method) {
    for (int i = 0; i < _count; i++) {
        if (_blobs[i]._start == start && _blobs[i]._method == method) {
            _blobs[i]._method = NULL;
            return;
        }
    }
}

void CodeCache::sort() {
    qsort(_blobs, _count, sizeof(CodeBlob), CodeBlob::comparator);
}

jmethodID CodeCache::linear_search(const void* address) {
    for (int i = 0; i < _count; i++) {
        if (address >= _blobs[i]._start && address < _blobs[i]._end) {
            return _blobs[i]._method;
        }
    }
    return asJavaMethod(_name);
}

jmethodID CodeCache::binary_search(const void* address) {
    int low = 0;
    int high = _count - 1;

    while (low <= high) {
        int mid = (unsigned int)(low + high) >> 1;
        if (_blobs[mid]._end <= address) {
            low = mid + 1;
        } else if (_blobs[mid]._start > address) {
            high = mid - 1;
        } else {
            return _blobs[mid]._method;
        }
    }

    return low > 0 ? _blobs[low - 1]._method : asJavaMethod(_name);
}
