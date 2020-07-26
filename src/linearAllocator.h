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

#ifndef _LINEARALLOCATOR_H
#define _LINEARALLOCATOR_H

#include <stddef.h>


struct Chunk {
    Chunk* prev;
    volatile size_t offs;
    // To avoid false sharing
    char _padding[56];
};

class LinearAllocator {
  private:
    size_t _chunk_size;
    Chunk* _tail;
    Chunk* _reserve;

    Chunk* allocateChunk(Chunk* current);
    void freeChunk(Chunk* current);
    void reserveChunk(Chunk* current);
    Chunk* getNextChunk(Chunk* current);

  public:
    LinearAllocator(size_t chunk_size);
    ~LinearAllocator();

    void clear();

    void* alloc(size_t size);
};

#endif // _LINEARALLOCATOR_H
