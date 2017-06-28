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

#include <stdlib.h>
#include <string.h>
#include "codeCache.h"


void CodeCache::expand() {
    CodeBlob* old_blobs = _blobs;
    CodeBlob* new_blobs = new CodeBlob[_capacity * 2];
    memcpy(new_blobs, old_blobs, _capacity * sizeof(CodeBlob));
    _capacity *= 2;
    _blobs = new_blobs;
    delete[] old_blobs;
}

void CodeCache::add(const void* start, int length, jmethodID method) {
    if (_count >= _capacity) {
        expand();
    }

    _blobs[_count]._start = start;
    _blobs[_count]._end = (const char*)start + length;
    _blobs[_count]._method = method;
    _count++;
}

void CodeCache::remove(const void* start, jmethodID method) {
    for (int i = 0; i < _count; i++) {
        if (_blobs[i]._start == start && _blobs[i]._method == method) {
            _blobs[i]._method = NULL;
            return;
        }
    }
}

jmethodID CodeCache::find(const void* address) {
    for (int i = 0; i < _count; i++) {
        if (address >= _blobs[i]._start && address < _blobs[i]._end) {
            return _blobs[i]._method;
        }
    }
    return NULL;
}


NativeCodeCache::~NativeCodeCache() {
    for (int i = 0; i < _count; i++) {
        free(_blobs[i]._method);
    }
}

void NativeCodeCache::add(const void* start, int length, const char* name) {
    CodeCache::add(start, length, (jmethodID)strdup(name));
}

const char* NativeCodeCache::binary_search(const void* address) {
    int low = 0;
    int high = _count - 1;

    while (low <= high) {
        int mid = (unsigned int)(low + high) >> 1;
        if (_blobs[mid]._end <= address) {
            low = mid + 1;
        } else if (_blobs[mid]._start > address) {
            high = mid - 1;
        } else {
            return (const char*)_blobs[mid]._method;
        }
    }

    // Symbols with zero size can be valid functions: e.g. ASM entry points or kernel code
    if (low > 0 && _blobs[low - 1]._start == _blobs[low - 1]._end) {
        return (const char*)_blobs[low - 1]._method;
    }
    return _name;
}

void NativeCodeCache::sort() {
    if (_count == 0) return;
    
    qsort(_blobs, _count, sizeof(CodeBlob), CodeBlob::comparator);

    if (_min_address == NULL) _min_address = _blobs[0]._start;
    if (_max_address == NULL) _max_address = _blobs[_count - 1]._end;
}
