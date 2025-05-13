/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf.h"
#include <string.h>

ProtobufBuffer::~ProtobufBuffer() {
  if (_parent_message == nullptr) {
    return;
  }
  _parent_message->commitMessage(offset());
}

void ProtobufBuffer::putVarInt(u32 n) {
  _offset = putVarInt(_offset, n);
}

void ProtobufBuffer::putVarInt(u64 n) {
  _offset = putVarInt(_offset, n);
}

size_t ProtobufBuffer::putVarInt(size_t offset, u32 n) {
  while ((n >> 7) != 0) {
    _data[offset++] = (unsigned char) (0x80 | (n & 0x7f));
    n >>= 7;
  }
  _data[offset++] = (unsigned char)n;
  return offset;
}

size_t ProtobufBuffer::putVarInt(size_t offset, u64 n) {
  while ((n >> 7) != 0) {
    _data[offset++] = (unsigned char) (0x80 | (n & 0x7f));
    n >>= 7;
  }
  _data[offset++] = (unsigned char) n;
  return offset;
}

void ProtobufBuffer::tag(protobuf_index_t index, protobuf_t type) {
  putVarInt((u32) index << 3 | type);
}

void ProtobufBuffer::field(protobuf_index_t index, bool b) {
  field(index, (u32)b);
}

void ProtobufBuffer::field(protobuf_index_t index, u32 n) {
  tag(index, VARINT);
  putVarInt(n);
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
  memcpy(_data + _offset, s, len);
  _offset += len;
}

ProtobufBuffer ProtobufBuffer::startMessage(protobuf_index_t index) {
  tag(index, LEN);
  _offset += nested_field_byte_count;
  return ProtobufBuffer(this);
}

void ProtobufBuffer::commitMessage(size_t message_length) {
  size_t message_length_encode = message_length;
  for (size_t i = 0; i < nested_field_byte_count - 1; ++i) {
    size_t idx = _offset - nested_field_byte_count + i;
    _data[idx] = (unsigned char) (0x80 | (message_length_encode & 0x7f));
    message_length_encode >>= 7;
  }
  _data[_offset - 1] = (unsigned char) message_length_encode;
  _offset += message_length;
}
