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

#include "linearAllocator.h"
#include "os.h"


LinearAllocator::LinearAllocator(size_t chunk_size) {
    _chunk_size = chunk_size;
    _reserve = _tail = allocateChunk(NULL);
}

LinearAllocator::~LinearAllocator() {
    clear();
    freeChunk(_tail);
}

void LinearAllocator::clear() {
    if (_reserve->prev == _tail) {
        freeChunk(_reserve);
    }
    while (_tail->prev != NULL) {
        Chunk* current = _tail;
        _tail = _tail->prev;
        freeChunk(current);
    }
    _reserve = _tail;
    _tail->offs = sizeof(Chunk);
}

void* LinearAllocator::alloc(size_t size) {
    Chunk* chunk = _tail;

    do {
        // Fast path: bump a pointer with CAS
        for (size_t offs = chunk->offs; offs + size <= _chunk_size; offs = chunk->offs) {
            if (__sync_bool_compare_and_swap(&chunk->offs, offs, offs + size)) {
                if (_chunk_size / 2 - offs < size) {
                    // Stepped over a middle of the chunk - it's time to prepare a new one
                    reserveChunk(chunk);
                }
                return (char*)chunk + offs;
            }
        }
    } while ((chunk = getNextChunk(chunk)) != NULL);

    return NULL;
}

Chunk* LinearAllocator::allocateChunk(Chunk* current) {
    Chunk* chunk = (Chunk*)OS::safeAlloc(_chunk_size);
    if (chunk != NULL) {
        chunk->prev = current;
        chunk->offs = sizeof(Chunk);
    }
    return chunk;
}

void LinearAllocator::freeChunk(Chunk* current) {
    OS::safeFree(current, _chunk_size);
}

void LinearAllocator::reserveChunk(Chunk* current) {
    Chunk* reserve = allocateChunk(current);
    if (reserve != NULL && !__sync_bool_compare_and_swap(&_reserve, current, reserve)) {
        // Unlikely case that we are too late
        freeChunk(reserve);
    }
}

Chunk* LinearAllocator::getNextChunk(Chunk* current) {
    Chunk* reserve = _reserve;

    if (reserve == current) {
        // Unlikely case: no reserve yet.
        // It's probably being allocated right now, so let's compete
        reserve = allocateChunk(current);
        if (reserve == NULL) {
            // Not enough memory
            return NULL;
        }

        Chunk* prev_reserve = __sync_val_compare_and_swap(&_reserve, current, reserve);
        if (prev_reserve != current) {
            freeChunk(reserve);
            reserve = prev_reserve;
        }
    }

    // Expected case: a new chunk is already reserved
    Chunk* tail = __sync_val_compare_and_swap(&_tail, current, reserve);
    return tail == current ? reserve : tail;
}
