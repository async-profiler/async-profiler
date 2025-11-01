/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "dictionary.h"
#include "arch.h"


static inline char* allocateKey(const char* key, size_t length) {
    char* result = (char*)malloc(length + 1);
    memcpy(result, key, length);
    result[length] = 0;
    return result;
}

static inline bool keyEquals(const char* candidate, const char* key, size_t length) {
    return strncmp(candidate, key, length) == 0 && candidate[length] == 0;
}


Dictionary::Dictionary() {
    _table = (DictTable*)calloc(1, sizeof(DictTable));
    _base_index = _table->base_index = 1;
}

Dictionary::~Dictionary() {
    clear(_table);
    free(_table);
}

void Dictionary::clear() {
    clear(_table);
    memset(_table, 0, sizeof(DictTable));
    _base_index = _table->base_index = 1;
}

void Dictionary::clear(DictTable* table) {
    for (int i = 0; i < ROWS; i++) {
        DictRow* row = &table->rows[i];
        for (int j = 0; j < CELLS; j++) {
            free(row->keys[j]);
        }
        if (row->next != NULL) {
            clear(row->next);
            free(row->next);
        }
    }
}

size_t Dictionary::usedMemory() {
    return _table != NULL ? usedMemory(_table) : 0;
}

size_t Dictionary::usedMemory(DictTable* table) {
    size_t bytes = sizeof(DictTable);
    for (int i = 0; i < ROWS; i++) {
        DictRow* row = &table->rows[i];
        if (row->next != NULL) {
            bytes += usedMemory(row->next);
        }
    }
    return bytes;
}

// Many popular symbols are quite short, e.g. "[B", "()V" etc.
// FNV-1a is reasonably fast and sufficiently random.
unsigned int Dictionary::hash(const char* key, size_t length) {
    unsigned int h = 2166136261U;
    for (size_t i = 0; i < length; i++) {
        h = (h ^ key[i]) * 16777619;
    }
    return h;
}

unsigned int Dictionary::lookup(const char* key) {
    return lookup(key, strlen(key));
}

unsigned int Dictionary::lookup(const char* key, size_t length) {
    DictTable* table = _table;
    unsigned int h = hash(key, length);

    while (true) {
        DictRow* row = &table->rows[h % ROWS];
        for (int c = 0; c < CELLS; c++) {
            if (row->keys[c] == NULL) {
                char* new_key = allocateKey(key, length);
                if (__sync_bool_compare_and_swap(&row->keys[c], NULL, new_key)) {
                    return table->index(h % ROWS, c);
                }
                free(new_key);
            }
            if (keyEquals(row->keys[c], key, length)) {
                return table->index(h % ROWS, c);
            }
        }

        if (row->next == NULL) {
            DictTable* new_table = (DictTable*)calloc(1, sizeof(DictTable));
            new_table->base_index = __sync_add_and_fetch(&_base_index, TABLE_CAPACITY);
            if (!__sync_bool_compare_and_swap(&row->next, NULL, new_table)) {
                free(new_table);
            }
        }

        table = row->next;
        h = (h >> ROW_BITS) | (h << (32 - ROW_BITS));
    }
}

void Dictionary::collect(std::map<unsigned int, const char*>& map) {
    collect(map, _table);
}

void Dictionary::collect(std::map<unsigned int, const char*>& map, DictTable* table) {
    for (int i = 0; i < ROWS; i++) {
        DictRow* row = &table->rows[i];
        for (int j = 0; j < CELLS; j++) {
            if (row->keys[j] != NULL) {
                map[table->index(i, j)] = row->keys[j];
            }
        }
        if (row->next != NULL) {
            collect(map, row->next);
        }
    }
}
