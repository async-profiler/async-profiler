/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
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

size_t LinearAllocator::usedMemory() {
    size_t bytes = _reserve->prev == _tail ? _chunk_size : 0;
    for (Chunk* chunk = _tail; chunk != NULL; chunk = chunk->prev) {
        bytes += _chunk_size;
    }
    return bytes;
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
