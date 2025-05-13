/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf.h"
#include <string.h>

static size_t computeVarIntByteSize(u64 value) {
  if (value <= 0x7F) return 1;
  else if (value <= 0x3FFF) return 2;
  else if (value <= 0x1FFFFF) return 3;
  else if (value <= 0xFFFFFFF) return 4;
  else if (value <= 0x7FFFFFFFF) return 5;
  else if (value <= 0x3FFFFFFFFFF) return 6;
  else if (value <= 0x1FFFFFFFFFFFF) return 7;
  else if (value <= 0xFFFFFFFFFFFFFF) return 8;
  else if (value <= 0x7FFFFFFFFFFFFFFF) return 9;
  return 10;
}

void ProtobufBuffer::ensureCapacity(size_t new_data_size) {
  size_t expected_capacity = _offset + new_data_size;
  while (expected_capacity > _capacity) {
    _capacity *= 2;
  }
  _data = (unsigned char*) realloc(_data, _capacity * sizeof(unsigned char));
}

void ProtobufBuffer::putVarInt(u64 n) {
  _offset = putVarInt(_offset, n);
}

size_t ProtobufBuffer::putVarInt(size_t offset, u64 n) {
  ensureCapacity(computeVarIntByteSize(n));
  while ((n >> 7) != 0) {
    _data[offset++] = (unsigned char) (0x80 | (n & 0x7f));
    n >>= 7;
  }
  _data[offset++] = (unsigned char) n;
  return offset;
}

void ProtobufBuffer::tag(protobuf_index_t index, protobuf_t type) {
  putVarInt((u64) (index << 3 | type));
}

void ProtobufBuffer::field(protobuf_index_t index, bool b) {
  field(index, (u64) b);
}

void ProtobufBuffer::field(protobuf_index_t index, u64 n) {
  tag(index, VARINT);
  putVarInt(n);
}

void ProtobufBuffer::field(protobuf_index_t index, const char* s) {
  field(index, s, strlen(s));
}

void ProtobufBuffer::field(protobuf_index_t index, const char* s, size_t len) {
  field(index, (const unsigned char*) s, strlen(s));
}

void ProtobufBuffer::field(protobuf_index_t index, const unsigned char* s, size_t len) {
  tag(index, LEN);
  putVarInt((u64) len);

  ensureCapacity(len);
  memcpy(_data + _offset, s, len);
  _offset += len;
}

protobuf_mark_t ProtobufBuffer::startMessage(protobuf_index_t index) {
  tag(index, LEN);

  ensureCapacity(nested_field_byte_count);
  _offset += nested_field_byte_count;
  return _offset;
}

void ProtobufBuffer::commitMessage(protobuf_mark_t mark) {
  size_t message_length = _offset - mark;
  for (size_t i = 0; i < nested_field_byte_count - 1; ++i) {
    size_t idx = mark - nested_field_byte_count + i;
    _data[idx] = (unsigned char) (0x80 | (message_length & 0x7f));
    message_length >>= 7;
  }
  _data[mark - 1] = (unsigned char) message_length;
}
