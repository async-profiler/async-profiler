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
#include "vmEntry.h"

const int INITIAL_CODE_CACHE_CAPACITY = 1024;


class CodeBlob {
  private:
    void* _id;
    const void* _start;
    unsigned    _length;
    bool        _isNmethod;
  public:
    const void* start() { return _start; }
    const void* end() { return (const char*)_start + _length; }
    void init(const void* s, unsigned l) { _start = s; _length = l; }

    bool isNmethod() { return _isNmethod; }
    void setName(const char *n) { _id = (void *) n; _isNmethod = false; }
    void setMethod(jmethodID m) { _id = m; _isNmethod = true; }
    char *getName() { return isNmethod() ? NULL : (char *)_id; }
    jmethodID getMethod() { return isNmethod() ? (jmethodID)_id : NULL; }

    static int comparator(const void* c1, const void* c2) {
        CodeBlob* cb1 = (CodeBlob*)c1;
        CodeBlob* cb2 = (CodeBlob*)c2;
        if (cb1->_start < cb2->_start) {
            return -1;
        } else if (cb1->_start > cb2->_start) {
            return 1;
        } else if (cb1->end() == cb2->end()) {
            return 0;
        } else {
            return cb1->end() > cb2->end() ? -1 : 1;
        }
    }
};

// Base class for models of native libs and the VM's code cache
class CodeUnit {
  protected:
    int _capacity;
    int _count;
    CodeBlob* _blobs;
    const void* _min_address;
    const void* _max_address;
    const int binarySearchPosition(const void* addr);

    void expand();

  public:
    CodeUnit() {
        _capacity = INITIAL_CODE_CACHE_CAPACITY;
        _count = 0;
        _blobs = new CodeBlob[_capacity];
        _min_address = (void *)(-1l);
        _max_address = (void *)(0l);
    }

    ~CodeUnit() {
        delete[] _blobs;
    }

    CodeBlob *add(const void* start, int length, bool insert);
    CodeBlob *find(const void* address) {
        int pos = binarySearchPosition(address);
        return pos >= 0 ? _blobs + pos : NULL;
    }
    bool contains(const void* address) {
        return address >= _min_address && address < _max_address;
    }
    const void *minAddr() { return _min_address; }
    const void *maxAddr() { return _max_address; }
};

// Model for native libs
class NativeLib : public CodeUnit {
  private:
    char* _name;
    bool _is_kernel;

  public:
    NativeLib(const char* name, const void* min_address = (void *) (-1l), const void* max_address = NULL);
    bool isKernel() { return _is_kernel; }
    void setIsKernel() { _is_kernel = true; }

    ~NativeLib();

    const char* name() {
        return _name;
    }

    void add(const void* start, int length, const char* name);
    void sort();
    const char* binarySearch(const void* address);
    const void* findSymbol(const char* name);
    const void* findSymbolByPrefix(const char* prefix);
};

// Model of the VM's code cache
class VmCodeCache : public CodeUnit {
    const void *_interp_min;
    const void *_interp_max;
  public:
    void add(const void* start, int length, const jmethodID method);
    void add(const void* start, int length, const char *name);
    void remove(const void* start, jmethodID method);
    bool isInterpreter(const void *pc) { return (pc >= _interp_min) && (pc < _interp_max); }
};

#endif // _CODECACHE_H
