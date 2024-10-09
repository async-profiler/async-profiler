/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "threadFilter.h"
#include "os.h"


ThreadFilter::ThreadFilter() {
    memset(_bitmap, 0, sizeof(_bitmap));
    _bitmap[0] = (u32*)OS::safeAlloc(BITMAP_SIZE);

    _enabled = false;
    _size = 0;
}

ThreadFilter::~ThreadFilter() {
    for (int i = 0; i < MAX_BITMAPS; i++) {
        if (_bitmap[i] != NULL) {
            OS::safeFree(_bitmap[i], BITMAP_SIZE);
        }
    }
}

void ThreadFilter::init(const char* filter) {
    if (filter == NULL) {
        _enabled = false;
        return;
    }

    char* end;
    do {
        int id = strtol(filter, &end, 0);
        if (id <= 0) {
            break;
        }

        if (*end == '-') {
            int to = strtol(end + 1, &end, 0);
            while (id <= to) {
                add(id++);
            }
        } else {
            add(id);
        }

        filter = end + 1;
    } while (*end);

    _enabled = true;
}

void ThreadFilter::clear() {
    for (int i = 0; i < MAX_BITMAPS; i++) {
        if (_bitmap[i] != NULL) {
            memset(_bitmap[i], 0, BITMAP_SIZE);
        }
    }
    _size = 0;
}

size_t ThreadFilter::usedMemory() {
    size_t bytes = 0;
    for (int i = 0; i < MAX_BITMAPS; i++) {
        if (_bitmap[i] != NULL) {
            bytes += BITMAP_SIZE;
        }
    }
    return bytes;
}

bool ThreadFilter::accept(int thread_id) {
    u32* b = bitmap(thread_id);
    return b != NULL && (word(b, thread_id) & (1 << (thread_id & 0x1f)));
}

void ThreadFilter::add(int thread_id) {
    u32* b = bitmap(thread_id);
    if (b == NULL) {
        b = (u32*)OS::safeAlloc(BITMAP_SIZE);
        u32* oldb = __sync_val_compare_and_swap(&_bitmap[(u32)thread_id / BITMAP_CAPACITY], NULL, b);
        if (oldb != NULL) {
            OS::safeFree(b, BITMAP_SIZE);
            b = oldb;
        }
    }

    u32 bit = 1 << (thread_id & 0x1f);
    if (!(__sync_fetch_and_or(&word(b, thread_id), bit) & bit)) {
        atomicInc(_size);
    }
}

void ThreadFilter::remove(int thread_id) {
    u32* b = bitmap(thread_id);
    if (b == NULL) {
        return;
    }

    u32 bit = 1 << (thread_id & 0x1f);
    if (__sync_fetch_and_and(&word(b, thread_id), ~bit) & bit) {
        atomicInc(_size, -1);
    }
}

void ThreadFilter::collect(std::vector<int>& v) {
    for (int i = 0; i < MAX_BITMAPS; i++) {
        u32* b = _bitmap[i];
        if (b != NULL) {
            int start_id = i * BITMAP_CAPACITY;
            for (int j = 0; j < BITMAP_SIZE / sizeof(u32); j++) {
                u32 word = b[j];
                if (word) {
                    for (int bit = 0; bit < 32; bit++) {
                        if (word & (1 << bit)) {
                            v.push_back(start_id + j * 32 + bit);
                        }
                    }
                }
            }
        }
    }
}
