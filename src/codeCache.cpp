/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "codeCache.h"
#include "dwarf.h"
#include "os.h"


char* NativeFunc::create(const char* name, short lib_index) {
    NativeFunc* f = (NativeFunc*)malloc(sizeof(NativeFunc) + 1 + strlen(name));
    f->_lib_index = lib_index;
    f->_mark = 0;
    return strcpy(f->_name, name);
}

void NativeFunc::destroy(char* name) {
    free(from(name));
}

size_t NativeFunc::usedMemory(const char* name) {
    return sizeof(NativeFunc) + 1 + strlen(from(name)->_name);
}


CodeCache::CodeCache(const char* name, short lib_index, bool imports_patchable,
                     const void* min_address, const void* max_address) {
    _name = NativeFunc::create(name, -1);
    _lib_index = lib_index;
    _min_address = min_address;
    _max_address = max_address;
    _text_base = NULL;

    _plt_offset = 0;
    _plt_size = 0;

    memset(_imports, 0, sizeof(_imports));
    _imports_patchable = imports_patchable;
    _debug_symbols = false;

    _dwarf_table = NULL;
    _dwarf_table_length = 0;

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
    free(_dwarf_table);
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

void CodeCache::mark(NamePredicate predicate, char value) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = _blobs[i]._name;
        if (blob_name != NULL && predicate(blob_name)) {
            NativeFunc::mark(blob_name, value);
        }
    }

    if (value == MARK_VM_RUNTIME && _name != NULL) {
        // In case a library has no debug symbols
        NativeFunc::mark(_name, value);
    }
}

CodeBlob* CodeCache::findBlob(const char* name) {
    for (int i = 0; i < _count; i++) {
        const char* blob_name = _blobs[i]._name;
        if (blob_name != NULL && strcmp(blob_name, name) == 0) {
            return &_blobs[i];
        }
    }
    return NULL;
}

CodeBlob* CodeCache::findBlobByAddress(const void* address) {
    for (int i = 0; i < _count; i++) {
        if (address >= _blobs[i]._start && address < _blobs[i]._end) {
            return &_blobs[i];
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
    CodeBlob* blob = findBlob(name);
    return blob == NULL ? NULL : blob->_start;
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

void CodeCache::addImport(void** entry, const char* name) {
    switch (name[0]) {
        case 'd':
            if (strcmp(name, "dlopen") == 0) {
                _imports[im_dlopen] = entry;
            }
            break;
        case 'p':
            if (strcmp(name, "pthread_create") == 0) {
                _imports[im_pthread_create] = entry;
            } else if (strcmp(name, "pthread_exit") == 0) {
                _imports[im_pthread_exit] = entry;
            } else if (strcmp(name, "pthread_setspecific") == 0) {
                _imports[im_pthread_setspecific] = entry;
            } else if (strcmp(name, "poll") == 0) {
                _imports[im_poll] = entry;
            }
            break;
    }
}

void** CodeCache::findImport(ImportId id) {
    if (!_imports_patchable) {
        makeImportsPatchable();
        _imports_patchable = true;
    }
    return _imports[id];
}

void CodeCache::patchImport(ImportId id, void* hook_func) {
    void** entry = findImport(id);
    if (entry != NULL) {
        *entry = hook_func;
    }
}

void CodeCache::makeImportsPatchable() {
    void** min_import = (void**)-1;
    void** max_import = NULL;
    for (int i = 0; i < NUM_IMPORTS; i++) {
        if (_imports[i] != NULL && _imports[i] < min_import) min_import = _imports[i];
        if (_imports[i] != NULL && _imports[i] > max_import) max_import = _imports[i];
    }

    if (max_import != NULL) {
        uintptr_t patch_start = (uintptr_t)min_import & ~OS::page_mask;
        uintptr_t patch_end = (uintptr_t)max_import & ~OS::page_mask;
        mprotect((void*)patch_start, patch_end - patch_start + OS::page_size, PROT_READ | PROT_WRITE);
    }
}

void CodeCache::setDwarfTable(FrameDesc* table, int length) {
    _dwarf_table = table;
    _dwarf_table_length = length;
}

FrameDesc* CodeCache::findFrameDesc(const void* pc) {
    u32 target_loc = (const char*)pc - _text_base;
    int low = 0;
    int high = _dwarf_table_length - 1;

    while (low <= high) {
        int mid = (unsigned int)(low + high) >> 1;
        if (_dwarf_table[mid].loc < target_loc) {
            low = mid + 1;
        } else if (_dwarf_table[mid].loc > target_loc) {
            high = mid - 1;
        } else {
            return &_dwarf_table[mid];
        }
    }

    if (low > 0) {
        return &_dwarf_table[low - 1];
    } else if (target_loc - _plt_offset < _plt_size) {
        return &FrameDesc::empty_frame;
    } else {
        return &FrameDesc::default_frame;
    }
}

size_t CodeCache::usedMemory() {
    size_t bytes = _capacity * sizeof(CodeBlob);
    bytes += _dwarf_table_length * sizeof(FrameDesc);
    bytes += NativeFunc::usedMemory(_name);
    for (int i = 0; i < _count; i++) {
        bytes += NativeFunc::usedMemory(_blobs[i]._name);
    }
    return bytes;
}
