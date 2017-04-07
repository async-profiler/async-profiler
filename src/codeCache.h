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

#ifndef _CODECACHE_H
#define _CODECACHE_H

#include <jvmti.h>


const int STRING_TABLE_CHUNK = 32000;
const int INITIAL_CODE_CACHE_CAPACITY = 1000;


class MethodName {
  private:
    char _buf[520];
    const char* _str;

    char* fixClassName(char* name);
    char* demangle(char* name);

  public:
    MethodName(jmethodID method, const char* sep = ".");

    const char* toString() { return _str; }
};


class StringTable {
  private:
    StringTable* _next;
    int _capacity;
    int _size;
    char _data[0];

  public:
    static StringTable* allocate(StringTable* next, int capacity, int size = 0);
    StringTable* destroy();

    char* data()    { return _data; }
    int remaining() { return _capacity - _size; }

    const char* add(const char* s, int length);
};


class CodeBlob {
  private:
    const void* _start;
    const void* _end;
    jmethodID _method;

  public:
    static int comparator(const void* c1, const void* c2) {
        CodeBlob* cb1 = (CodeBlob*)c1;
        CodeBlob* cb2 = (CodeBlob*)c2;
        if (cb1->_start < cb2->_start) {
            return -1;
        } else if (cb1->_start > cb2->_start) {
            return 1;
        } else if (cb1->_end == cb2->_end) {
            return 0;
        } else {
            return cb1->_end > cb2->_end ? -1 : 1;
        }
    }

    friend class CodeCache;
};


class CodeCache {
  private:
    char* _name;
    int _capacity;
    int _count;
    CodeBlob* _blobs;
    StringTable* _strings;
    const void* _min_address;
    const void* _max_address;

  public:
    CodeCache(const char* name,
              const void* min_address = (const void*)-1,
              const void* max_address = (const void*)0);
    ~CodeCache();

    const char* addString(const char* s);
    const char* addStrings(const char* s, int length);

    void add(const void* start, int length, jmethodID method);
    void add(const void* start, int length, const char* name);
    void remove(const void* start, jmethodID method);

    void sort();
    jmethodID linear_search(const void* address);
    jmethodID binary_search(const void* address);

    bool contains(const void* address) {
        return address >= _min_address && address < _max_address;
    }
};

#endif // _CODECACHE_H
