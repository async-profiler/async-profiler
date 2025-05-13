/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PROTOBUF_H
#define _PROTOBUF_H

#include "arch.h"
#include <string.h>
#include <type_traits>

typedef const u32 protobuf_t;
static protobuf_t VARINT = 0;
static protobuf_t LEN = 2;

typedef u32 protobuf_index_t;

// We assume the length of a nested field can be represented with 3 varint
// bytes.
const size_t nested_field_byte_count = 3;

class ProtobufBuffer {
private:
  ProtobufBuffer *_parent_message;
  unsigned char *_data;
  size_t _offset;

  ProtobufBuffer(ProtobufBuffer *parent) :
    _data(parent->_data + parent->offset()),
    _offset(0),
    _parent_message(parent) {}

  void putVarInt(u32 n);
  size_t putVarInt(size_t offset, u32 n);
  void putVarInt(u64 n);
  size_t putVarInt(size_t offset, u64 n);

  void tag(protobuf_index_t index, protobuf_t type);

  void commitMessage(size_t message_length);

public:
  ProtobufBuffer(unsigned char *data) :
    _data(data),
    _offset(0),
    _parent_message(nullptr) {}
  ~ProtobufBuffer();

  const unsigned char *data() const { return _data; }

  size_t offset() const { return _offset; }

  // VARINT
  void field(protobuf_index_t index, bool b);
  void field(protobuf_index_t index, u32 n);
  void field(protobuf_index_t index, u64 n);
  // LEN
  void field(protobuf_index_t index, const char *s);
  void field(protobuf_index_t index, const char *s, size_t len);
  void field(protobuf_index_t index, const unsigned char *s, size_t len);

  ProtobufBuffer startMessage(protobuf_index_t index);
};

#endif // _PROTOBUF_H
