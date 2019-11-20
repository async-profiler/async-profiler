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
#include "vmEntry.h"

void CodeUnit::expand() {
    CodeBlob* old_blobs = _blobs;
    CodeBlob* new_blobs = new CodeBlob[_capacity * 2];
    memcpy(new_blobs, old_blobs, _capacity * sizeof(CodeBlob));
    _capacity *= 2;
    _blobs = new_blobs;
    delete[] old_blobs;
}

CodeBlob *CodeUnit::add(const void* start, int length, bool insert) {
    if (_count >= _capacity) {
        expand();
    }

    if (!insert || _count == 0 || _blobs[_count - 1].end() <= start) {
        CodeBlob *blob = _blobs + _count;
        blob->init(start, length);
        _count++;
        return blob;
    }
    int ret = binarySearchPosition(start);
    if (ret >= 0) {
        return &_blobs[ret];
    }
    int pos =  -ret - 1;
    memmove(_blobs + pos + 1, _blobs + pos, (_count - pos) * sizeof(CodeBlob));
    CodeBlob *blob = _blobs + pos;
    blob->init(start, length);
    _count++;
    return blob;
}

void VmCodeCache::remove(const void* start, jmethodID method) {
    int pos = binarySearchPosition(start);
    if (_count > (pos + 1)) {
        memmove(_blobs + pos, _blobs + (pos + 1), (sizeof(CodeBlob)) * (_count - pos -1));
    }
    _count--;
}

NativeLib::NativeLib(const char* name, const void* min_address, const void* max_address) {
    _name = strdup(name);
    _min_address = min_address;
    _max_address = max_address;
    _is_kernel = false;
}

NativeLib::~NativeLib() {
    for (int i = 0; i < _count; i++) {
        free(_blobs[i].getName());
    }
    free(_name);
}

void NativeLib::add(const void* start, int length, const char* name) {
    char* name_copy = strdup(name);
    // Replace non-printable characters
    for (char* s = name_copy; *s != 0; s++) {
        if (*s < ' ') *s = '?';
    }
    CodeUnit::add(start, length, false)->setName(name_copy);
    if (start < _min_address) {
        _min_address = start;
    }
    if ((void*)((char*)start + length) > _max_address) {
        _max_address = (void*)((char*)start + length);
    }
}

void NativeLib::sort() {
    if (_count == 0) return;

    qsort(_blobs, _count, sizeof(CodeBlob), CodeBlob::comparator);

    if (_min_address == NULL) _min_address = _blobs[0].start();
    if (_max_address == NULL) _max_address = _blobs[_count - 1].end();
}

const int CodeUnit::binarySearchPosition(const void* addr) {
    int low = 0;
    int high = _count - 1;

    int mid;
    while (low <= high) {
        mid = (unsigned int)(low + high) >> 1;
        if (_blobs[mid].end() <= addr) {
            low = mid + 1;
        } else if (_blobs[mid].start() > addr) {
            high = mid - 1;
        } else if (_blobs[mid].start() <= addr && _blobs[mid].end() > addr) {
            return mid;
        }
    }

    // Symbols with zero size can be valid functions: e.g. ASM entry points or kernel code
    if (low > 0 && _blobs[low - 1].start() == _blobs[low - 1].end() && _blobs[low - 1].end() == addr) {
        return low - 1;
    }

    return -low - 1;
}


const char* NativeLib::binarySearch(const void* address) {
    int low = 0;
    int high = _count - 1;

    while (low <= high) {
        int mid = (unsigned int)(low + high) >> 1;
        if (_blobs[mid].end() <= address) {
            low = mid + 1;
        } else if (_blobs[mid].start() > address) {
            high = mid - 1;
        } else {
            return _blobs[mid].getName();
        }
    }

    // Symbols with zero size can be valid functions: e.g. ASM entry points or kernel code
    if (low > 0 && _blobs[low - 1].start() == _blobs[low - 1].end()) {
        return _blobs[low - 1].getName();
    }
    return _name;
}

const void* NativeLib::findSymbol(const char* name) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = _blobs[i].getName();
        if (blob_name != NULL && strcmp(blob_name, name) == 0) {
            return _blobs[i].start();
        }
    }
    return NULL;
}

const void* NativeLib::findSymbolByPrefix(const char* prefix) {
    int prefix_len = strlen(prefix);
    for (int i = 0; i < _count; i++) {
        const char* blob_name = _blobs[i].getName();
        if (blob_name != NULL && strncmp(blob_name, prefix, prefix_len) == 0) {
            return _blobs[i].start();
        }
    }
    return NULL;
}

void VmCodeCache::add(const void* start, int length, const jmethodID method) {
    CodeUnit::add(start, length, true)->setMethod(method);
    if (start < _min_address) {
        _min_address = start;
    }
    if ((void*)((char*)start + length) > _max_address) {
        _max_address = (void*)((char*)start + length);
    }
}

void VmCodeCache::add(const void* start, int length, const char *name) {
    CodeUnit::add(start, length, true)->setName(name);
    if (start < _min_address) {
        _min_address = start;
    }
    if ((void*)((char*)start + length) > _max_address) {
        _max_address = (void*)((char*)start + length);
    }
    if (strcmp(name, "Interpreter") == 0) {
        _interp_min = start;
        _interp_max = (const void*)((char*)start + length);
    }
}
