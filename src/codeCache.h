/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _CODECACHE_H
#define _CODECACHE_H

#include <jvmti.h>


#define NO_MIN_ADDRESS  ((const void*)-1)
#define NO_MAX_ADDRESS  ((const void*)0)

typedef bool (*NamePredicate)(const char* name);

const int INITIAL_CODE_CACHE_CAPACITY = 1000;
const int MAX_NATIVE_LIBS = 2048;


enum ImportId {
    im_dlopen,
    im_pthread_create,
    im_pthread_exit,
    im_pthread_setspecific,
    im_poll,
    NUM_IMPORTS
};

enum Mark {
    MARK_VM_RUNTIME = 1,
    MARK_INTERPRETER = 2,
    MARK_COMPILER_ENTRY = 3
};


class NativeFunc {
  private:
    short _lib_index;
    char _mark;
    char _reserved;
    char _name[0];

    static NativeFunc* from(const char* name) {
        return (NativeFunc*)(name - sizeof(NativeFunc));
    }

  public:
    static char* create(const char* name, short lib_index);
    static void destroy(char* name);

    static size_t usedMemory(const char* name);

    static short libIndex(const char* name) {
        return from(name)->_lib_index;
    }

    static char mark(const char* name) {
        return from(name)->_mark;
    }

    static void mark(const char* name, char value) {
        from(name)->_mark = value;
    }
};


class CodeBlob {
  public:
    const void* _start;
    const void* _end;
    char* _name;

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


class FrameDesc;

class CodeCache {
  private:
    char* _name;
    short _lib_index;
    const void* _min_address;
    const void* _max_address;
    const char* _text_base;

    unsigned int _plt_offset;
    unsigned int _plt_size;

    void** _imports[NUM_IMPORTS];
    bool _imports_patchable;
    bool _debug_symbols;

    FrameDesc* _dwarf_table;
    int _dwarf_table_length;

    int _capacity;
    int _count;
    CodeBlob* _blobs;

    void expand();
    void makeImportsPatchable();

  public:
    CodeCache(const char* name,
              short lib_index = -1,
              bool imports_patchable = false,
              const void* min_address = NO_MIN_ADDRESS,
              const void* max_address = NO_MAX_ADDRESS);

    ~CodeCache();

    const char* name() const {
        return _name;
    }

    const void* minAddress() const {
        return _min_address;
    }

    const void* maxAddress() const {
        return _max_address;
    }

    bool contains(const void* address) const {
        return address >= _min_address && address < _max_address;
    }

    void setTextBase(const char* text_base) {
        _text_base = text_base;
    }

    void setPlt(unsigned int plt_offset, unsigned int plt_size) {
        _plt_offset = plt_offset;
        _plt_size = plt_size;
    }

    bool hasDebugSymbols() const {
        return _debug_symbols;
    }

    void setDebugSymbols(bool debug_symbols) {
        _debug_symbols = debug_symbols;
    }

    void add(const void* start, int length, const char* name, bool update_bounds = false);
    void updateBounds(const void* start, const void* end);
    void sort();
    void mark(NamePredicate predicate, char value);

    void addImport(void** entry, const char* name);
    void** findImport(ImportId id);
    void patchImport(ImportId, void* hook_func);

    CodeBlob* findBlob(const char* name);
    CodeBlob* findBlobByAddress(const void* address);
    const char* binarySearch(const void* address);
    const void* findSymbol(const char* name);
    const void* findSymbolByPrefix(const char* prefix);
    const void* findSymbolByPrefix(const char* prefix, int prefix_len);

    void setDwarfTable(FrameDesc* table, int length);
    FrameDesc* findFrameDesc(const void* pc);

    size_t usedMemory();
};


class CodeCacheArray {
  private:
    CodeCache* _libs[MAX_NATIVE_LIBS];
    int _count;

  public:
    CodeCacheArray() : _count(0) {
    }

    CodeCache* operator[](int index) {
        return _libs[index];
    }

    int count() {
        return __atomic_load_n(&_count, __ATOMIC_ACQUIRE);
    }

    void add(CodeCache* lib) {
        int index = __atomic_load_n(&_count, __ATOMIC_ACQUIRE);
        _libs[index] = lib;
        __atomic_store_n(&_count, index + 1, __ATOMIC_RELEASE);
    }
};

#endif // _CODECACHE_H
