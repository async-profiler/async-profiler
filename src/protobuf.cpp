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

void ProtobufBuffer::tag(protobuf_index_t index, protobuf_t type) {
  // index is 3100 maximum
  // (https://protobuf.dev/programming-guides/proto-limits/)
  put8(index << 3 | type);
}

void ProtobufBuffer::field(protobuf_index_t index, bool b) {
  field(index, (u32)b);
}

void ProtobufBuffer::field(protobuf_index_t index, u32 n) {
  tag(index, VARINT);
  putVarInt<>(n);
}

void ProtobufBuffer::field(protobuf_index_t index, u64 n) {
  tag(index, VARINT);
  putVarInt<>(n);
}

void ProtobufBuffer::field(protobuf_index_t index, const char *s) {
  field(index, s, strlen(s));
}

void ProtobufBuffer::field(protobuf_index_t index, const char *s, size_t len) {
  tag(index, LEN);
  putVarInt<>(len);
  put(s, len);
}

void ProtobufBuffer::field(protobuf_index_t index, const ProtobufBuffer buffer, size_t len) {
  field(index, buffer.data(), buffer.offset());
}

ProtobufBuffer ProtobufBuffer::startMessage(protobuf_index_t index) {
  tag(index, LEN);
  skip(nested_field_byte_count);
  return ProtobufBuffer(this);
}

void ProtobufBuffer::commitMessage(size_t message_length) {
  size_t message_length_encode = message_length;
  for (size_t i = 0; i < nested_field_byte_count - 1; ++i) {
    _data[_offset - nested_field_byte_count + i] =
        (char)(0b10000000 | (message_length_encode & 0b01111111));
    message_length_encode >>= 7;
  }
  _data[_offset - 1] = (char)message_length_encode;
  skip(message_length);
}
