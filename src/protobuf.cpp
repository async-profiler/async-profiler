/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf.h"
#include <string.h>

void ProtobufBuffer::tag(int index, protobuf_t type) {
  // index is 3100 maximum
  // (https://protobuf.dev/programming-guides/proto-limits/)
  put8(index << 3 | type);
}

void ProtobufBuffer::field(int index, bool b) {
  field(index, (u32) b);
}

void ProtobufBuffer::field(int index, int n) {
  tag(index, I32);
  put32(n);
}

void ProtobufBuffer::field(int index, u32 n) {
  tag(index, VARINT);
  putVarInt<>(n);
}

void ProtobufBuffer::field(int index, long n) {
  tag(index, I64);
  put64(n);
}

void ProtobufBuffer::field(int index, u64 n) {
  tag(index, VARINT);
  putVarInt<>(n);
}

void ProtobufBuffer::field(int index, float n) {
  tag(index, I32);
  putFloat(n);
}

void ProtobufBuffer::field(int index, double n) {
  tag(index, I64);
  putDouble(n);
}

void ProtobufBuffer::field(int index, const char *s) {
  field(index, s, strlen(s));
}

void ProtobufBuffer::field(int index, const char *s, size_t len) {
  tag(index, LEN);
  putVarInt<>(len);
  put(s, len);
}

void ProtobufBuffer::field(int index, const ProtobufBuffer buffer, size_t len) {
  field(index, buffer.data(), buffer.offset());
}

size_t ProtobufBuffer::startField(int index) {
  tag(index, LEN);
  skip(nested_field_byte_count);
  return offset();
}

void ProtobufBuffer::commitField(size_t mark) {
  size_t length = offset() - mark;
  for (int i = 0; i < nested_field_byte_count - 1; ++i) {
    _data[mark - 3 + i] = (char)(0b10000000 | (length & 0b01111111));
    length >>= 7;
  }
  _data[mark - 1] = (char)length;
}

