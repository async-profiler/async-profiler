/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PROTOBUF_H
#define _PROTOBUF_H

#include "arch.h"
#include <string.h>
#include <type_traits>
#include <stdlib.h>

typedef const u32 protobuf_t;
static protobuf_t VARINT = 0;
static protobuf_t LEN = 2;

typedef u32 protobuf_index_t;
typedef u32 protobuf_mark_t;

// We assume the length of a nested field can be represented with 3 varint bytes.
const size_t nested_field_byte_count = 3;
const size_t minimum_initial_size = 16;

class ProtobufBuffer {
private:
  unsigned char* _data;
  size_t _capacity;
  size_t _offset;

  void putVarInt(u64 n);
  size_t putVarInt(size_t offset, u64 n);

  void tag(protobuf_index_t index, protobuf_t type);

  void ensureCapacity(size_t new_data_size);

public:
  ProtobufBuffer(size_t initial_capacity) : _offset(0) {
    if (initial_capacity < minimum_initial_size) {
      initial_capacity = minimum_initial_size;
    }
    _capacity = initial_capacity;
    _data = (unsigned char*) malloc(_capacity);
  }
  ~ProtobufBuffer() {
    free(_data);
  }

  const unsigned char* data() const { return _data; }

  size_t offset() const { return _offset; }
  size_t capacity() const { return _capacity; }

  // VARINT
  void field(protobuf_index_t index, bool b);
  void field(protobuf_index_t index, u64 n);
  // LEN
  void field(protobuf_index_t index, const char* s);
  void field(protobuf_index_t index, const char* s, size_t len);
  void field(protobuf_index_t index, const unsigned char* s, size_t len);

  protobuf_mark_t startMessage(protobuf_index_t index);
  void commitMessage(protobuf_mark_t mark);
};

#endif // _PROTOBUF_H
