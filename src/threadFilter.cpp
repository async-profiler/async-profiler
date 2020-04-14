/*
 * Copyright 2020 Andrei Pangin
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
#include <sys/mman.h>
#include "threadFilter.h"

    
ThreadFilter::ThreadFilter() {
    memset(_bitmap, 0, sizeof(_bitmap));
    _bitmap[0] = (u32*)mmap(NULL, BITMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    _enabled = false;
    _size = 0;
}

ThreadFilter::~ThreadFilter() {
    for (int i = 0; i < MAX_BITMAPS; i++) {
        if (_bitmap[i] != NULL) {
            munmap(_bitmap[i], BITMAP_SIZE);
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

bool ThreadFilter::accept(int thread_id) {
    u32* b = bitmap(thread_id);
    return b != NULL && (word(b, thread_id) & (1 << (thread_id & 0x1f)));
}

void ThreadFilter::add(int thread_id) {
    u32* b = bitmap(thread_id);
    if (b == NULL) {
        // Use mmap() rather than malloc() to allow calling from signal handler
        b = (u32*)mmap(NULL, BITMAP_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        u32* oldb = __sync_val_compare_and_swap(&_bitmap[(u32)thread_id / BITMAP_CAPACITY], NULL, b);
        if (oldb != NULL) {
            munmap(b, BITMAP_SIZE);
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

int ThreadFilter::collect(int* array, int max_count) {
    int count = 0;

    for (int i = 0; i < MAX_BITMAPS; i++) {
        u32* b = _bitmap[i];
        if (b != NULL) {
            int start_id = i * BITMAP_CAPACITY;
            for (int j = 0; j < BITMAP_SIZE / sizeof(u32); j++) {
                u32 word = b[j];
                if (word) {
                    for (int bit = 0; bit < 32; bit++) {
                        if (word & (1 << bit)) {
                            if (count >= max_count) return count;
                            array[count++] = start_id + j * 32 + bit;
                        }
                    }
                }
            }
        }
    }

    return count;
}
