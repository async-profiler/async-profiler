/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PROTOBUF_H
#define _PROTOBUF_H

#include <sys/types.h>
#include "arch.h"

typedef const u32 protobuf_t;
protobuf_t VARINT = 0;
protobuf_t LEN = 2;

typedef u32 protobuf_index_t;
typedef u32 protobuf_mark_t;

// We assume the length of a nested field can be represented with 3 varint bytes.
const size_t NESTED_FIELD_BYTE_COUNT = 3;
const size_t MINIMUM_INITIAL_SIZE = 16;

class ProtoBuffer {
  private:
    unsigned char* _data;
    size_t _capacity;
    size_t _offset;

    void putVarInt(u64 n);
    size_t putVarInt(size_t offset, u64 n);

    void tag(protobuf_index_t index, protobuf_t type);

    void ensureCapacity(size_t new_data_size);

  public:
    ProtoBuffer(size_t initial_capacity);
    ~ProtoBuffer();

    const unsigned char* data() const { return _data; }

    size_t offset() const { return _offset; }
    size_t capacity() const { return _capacity; }
    void reset() { _offset = 0; }

    // VARINT
    void field(protobuf_index_t index, bool b);
    void field(protobuf_index_t index, u32 n);
    void field(protobuf_index_t index, u64 n);
    // LEN
    void field(protobuf_index_t index, const char* s);
    void field(protobuf_index_t index, const char* s, size_t len);
    void field(protobuf_index_t index, const unsigned char* s, size_t len);

    protobuf_mark_t startMessage(protobuf_index_t index);
    void commitMessage(protobuf_mark_t mark);

    static size_t varIntSize(u64 value);
};

#endif // _PROTOBUF_H
