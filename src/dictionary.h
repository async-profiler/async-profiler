/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DICTIONARY_H
#define _DICTIONARY_H

#include <map>
#include <stddef.h>


#define ROW_BITS        7
#define ROWS            (1 << ROW_BITS)
#define CELLS           3
#define TABLE_CAPACITY  (ROWS * CELLS)


struct DictTable;

struct DictRow {
    char* keys[CELLS];
    DictTable* next;
};

struct DictTable {
    DictRow rows[ROWS];
    unsigned int base_index;

    unsigned int index(int row, int col) {
        return base_index + (col << ROW_BITS) + row;
    }
};

// Append-only concurrent hash table based on multi-level arrays
class Dictionary {
  private:
    DictTable* _table;
    volatile unsigned int _base_index;

    static void clear(DictTable* table);
    static size_t usedMemory(DictTable* table);

    static unsigned int hash(const char* key, size_t length);

    static void collect(std::map<unsigned int, const char*>& map, DictTable* table);

  public:
    Dictionary();
    ~Dictionary();

    void clear();
    size_t usedMemory();

    unsigned int lookup(const char* key);
    unsigned int lookup(const char* key, size_t length);

    void collect(std::map<unsigned int, const char*>& map);
};

#endif // _DICTIONARY_H
