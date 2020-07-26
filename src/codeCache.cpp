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

    int live = 0;
    for (int i = 0; i < _count; i++) {
        if (_blobs[i]._method != NULL) {
            new_blobs[live++] = _blobs[i];
        }
    }

    _count = live;
    _capacity *= 2;
    _blobs = new_blobs;
    delete[] old_blobs;
}

void CodeCache::add(const void* start, int length, jmethodID method, bool update_bounds) {
    if (_count >= _capacity) {
        expand();
    }

    const void* end = (const char*)start + length;
    _blobs[_count]._start = start;
    _blobs[_count]._end = end;
    _blobs[_count]._method = method;
    _count++;

    if (update_bounds) {
        if (start < _min_address) _min_address = start;
        if (end > _max_address) _max_address = end;
    }
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
        CodeBlob* cb = _blobs + i;
        if (address >= cb->_start && address < cb->_end && cb->_method != NULL) {
            return _blobs[i]._method;
        }
    }
    return NULL;
}


NativeCodeCache::NativeCodeCache(const char* name, const void* min_address, const void* max_address) {
    _name = strdup(name);
    _min_address = min_address;
    _max_address = max_address;
}

NativeCodeCache::~NativeCodeCache() {
    for (int i = 0; i < _count; i++) {
        free(_blobs[i]._method);
    }
    free(_name);
}

void NativeCodeCache::add(const void* start, int length, const char* name, bool update_bounds) {
    char* name_copy = strdup(name);
    // Replace non-printable characters
    for (char* s = name_copy; *s != 0; s++) {
        if (*s < ' ') *s = '?';
    }
    CodeCache::add(start, length, (jmethodID)name_copy, update_bounds);
}

void NativeCodeCache::sort() {
    if (_count == 0) return;

    qsort(_blobs, _count, sizeof(CodeBlob), CodeBlob::comparator);

    if (_min_address == NO_MIN_ADDRESS) _min_address = _blobs[0]._start;
    if (_max_address == NO_MAX_ADDRESS) _max_address = _blobs[_count - 1]._end;
}

const char* NativeCodeCache::binarySearch(const void* address) {
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

const void* NativeCodeCache::findSymbol(const char* name) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = (const char*)_blobs[i]._method;
        if (blob_name != NULL && strcmp(blob_name, name) == 0) {
            return _blobs[i]._start;
        }
    }
    return NULL;
}

const void* NativeCodeCache::findSymbolByPrefix(const char* prefix) {
    return findSymbolByPrefix(prefix, strlen(prefix));
}

const void* NativeCodeCache::findSymbolByPrefix(const char* prefix, int prefix_len) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = (const char*)_blobs[i]._method;
        if (blob_name != NULL && strncmp(blob_name, prefix, prefix_len) == 0) {
            return _blobs[i]._start;
        }
    }
    return NULL;
}
