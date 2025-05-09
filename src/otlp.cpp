/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "otlp.h"

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, void>::type ProtobufBuffer::putVarInt(T n) {
    _offset = putVarInt(_offset, n);
}

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, size_t>::type ProtobufBuffer::putVarInt(size_t offset, T n) {
    while ((n >> 7) != 0) {
        _data[offset++] = (char)(0b10000000 | (n & 0b01111111));
        n >>= 7;
    }
    _data[offset++] = (char)n;
    return offset;
}

void ProtobufBuffer::tag(int index, protobuf_t type) {
    // index is 3100 maximum (https://protobuf.dev/programming-guides/proto-limits/)
    put8(index << 3 | type);
}

void ProtobufBuffer::field(int index, bool b) {
    tag(index, VARINT);
    putVarInt<>((u32) b);
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

void ProtobufBuffer::field(int index, const char* s) {
    field(index, s, strlen(s));
}

void ProtobufBuffer::field(int index, const char* s, size_t len) {
    tag(index, LEN);
    put(s, len);
}

void ProtobufBuffer::field(int index, const LittleEndianBuffer buffer, size_t len) {
    field(index, buffer.data(), buffer.offset());
}

size_t ProtobufBuffer::startField(int index) {
    tag(index, LEN);
    skip(3);
    return offset();
}

void ProtobufBuffer::commitField(size_t mark) {
    size_t length = offset() - mark;
    putVarInt<>(mark - 3, (u32)length);
}
