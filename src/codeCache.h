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


#define NO_MIN_ADDRESS  ((const void*)-1)
#define NO_MAX_ADDRESS  ((const void*)0)

const int INITIAL_CODE_CACHE_CAPACITY = 1000;


class CodeBlob {
  public:
    const void* _start;
    const void* _end;
    jmethodID _method;

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
};


class CodeCache {
  protected:
    int _capacity;
    int _count;
    CodeBlob* _blobs;
    const void* _min_address;
    const void* _max_address;

    void expand();

  public:
    CodeCache() {
        _capacity = INITIAL_CODE_CACHE_CAPACITY;
        _count = 0;
        _blobs = new CodeBlob[_capacity];
        _min_address = NO_MIN_ADDRESS;
        _max_address = NO_MAX_ADDRESS;
    }

    ~CodeCache() {
        delete[] _blobs;
    }

    bool contains(const void* address) {
        return address >= _min_address && address < _max_address;
    }

    void add(const void* start, int length, jmethodID method, bool update_bounds = false);
    void remove(const void* start, jmethodID method);
    jmethodID find(const void* address);
};


class NativeCodeCache : public CodeCache {
  private:
    char* _name;

  public:
    NativeCodeCache(const char* name,
                    const void* min_address = NO_MIN_ADDRESS,
                    const void* max_address = NO_MAX_ADDRESS);

    ~NativeCodeCache();

    const char* name() {
        return _name;
    }

    void add(const void* start, int length, const char* name, bool update_bounds = false);
    void sort();
    const char* binarySearch(const void* address);
    const void* findSymbol(const char* name);
    const void* findSymbolByPrefix(const char* prefix);
    const void* findSymbolByPrefix(const char* prefix, int prefix_len);
};

#endif // _CODECACHE_H
