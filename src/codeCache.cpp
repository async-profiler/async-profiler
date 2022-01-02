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


char* NativeFunc::create(const char* name, short lib_index) {
    NativeFunc* f = (NativeFunc*)malloc(sizeof(NativeFunc) + 1 + strlen(name));
    f->_lib_index = lib_index;
    f->_mark = 0;
    return strcpy(f->_name, name);
}

void NativeFunc::destroy(char* name) {
    free(from(name));
}


CodeCache::CodeCache(const char* name, short lib_index, const void* min_address, const void* max_address) {
    _name = NativeFunc::create(name, -1);
    _lib_index = lib_index;
    _min_address = min_address;
    _max_address = max_address;

    _capacity = INITIAL_CODE_CACHE_CAPACITY;
    _count = 0;
    _blobs = new CodeBlob[_capacity];
}

CodeCache::~CodeCache() {
    for (int i = 0; i < _count; i++) {
        NativeFunc::destroy(_blobs[i]._name);
    }
    NativeFunc::destroy(_name);
    delete[] _blobs;
}

void CodeCache::expand() {
    CodeBlob* old_blobs = _blobs;
    CodeBlob* new_blobs = new CodeBlob[_capacity * 2];

    memcpy(new_blobs, old_blobs, _count * sizeof(CodeBlob));

    _capacity *= 2;
    _blobs = new_blobs;
    delete[] old_blobs;
}

void CodeCache::add(const void* start, int length, const char* name, bool update_bounds) {
    char* name_copy = NativeFunc::create(name, _lib_index);
    // Replace non-printable characters
    for (char* s = name_copy; *s != 0; s++) {
        if (*s < ' ') *s = '?';
    }

    if (_count >= _capacity) {
        expand();
    }

    const void* end = (const char*)start + length;
    _blobs[_count]._start = start;
    _blobs[_count]._end = end;
    _blobs[_count]._name = name_copy;
    _count++;

    if (update_bounds) {
        updateBounds(start, end);
    }
}

void CodeCache::updateBounds(const void* start, const void* end) {
    if (start < _min_address) _min_address = start;
    if (end > _max_address) _max_address = end;
}

void CodeCache::sort() {
    if (_count == 0) return;

    qsort(_blobs, _count, sizeof(CodeBlob), CodeBlob::comparator);

    if (_min_address == NO_MIN_ADDRESS) _min_address = _blobs[0]._start;
    if (_max_address == NO_MAX_ADDRESS) _max_address = _blobs[_count - 1]._end;
}

void CodeCache::mark(NamePredicate predicate) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = _blobs[i]._name;
        if (blob_name != NULL && predicate(blob_name)) {
            NativeFunc::mark(blob_name);
        }
    }
}

const char* CodeCache::find(const void* address) {
    for (int i = 0; i < _count; i++) {
        if (address >= _blobs[i]._start && address < _blobs[i]._end) {
            return _blobs[i]._name;
        }
    }
    return NULL;
}

const char* CodeCache::binarySearch(const void* address) {
    int low = 0;
    int high = _count - 1;

    while (low <= high) {
        int mid = (unsigned int)(low + high) >> 1;
        if (_blobs[mid]._end <= address) {
            low = mid + 1;
        } else if (_blobs[mid]._start > address) {
            high = mid - 1;
        } else {
            return _blobs[mid]._name;
        }
    }

    // Symbols with zero size can be valid functions: e.g. ASM entry points or kernel code.
    // Also, in some cases (endless loop) the return address may point beyond the function.
    if (low > 0 && (_blobs[low - 1]._start == _blobs[low - 1]._end || _blobs[low - 1]._end == address)) {
        return _blobs[low - 1]._name;
    }
    return _name;
}

const void* CodeCache::findSymbol(const char* name) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = _blobs[i]._name;
        if (blob_name != NULL && strcmp(blob_name, name) == 0) {
            return _blobs[i]._start;
        }
    }
    return NULL;
}

const void* CodeCache::findSymbolByPrefix(const char* prefix) {
    return findSymbolByPrefix(prefix, strlen(prefix));
}

const void* CodeCache::findSymbolByPrefix(const char* prefix, int prefix_len) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = _blobs[i]._name;
        if (blob_name != NULL && strncmp(blob_name, prefix, prefix_len) == 0) {
            return _blobs[i]._start;
        }
    }
    return NULL;
}
