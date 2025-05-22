/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include "protobuf.h"

ProtoBuffer::ProtoBuffer(size_t initial_capacity) : _offset(0) {
    _capacity = MAX(MINIMUM_INITIAL_SIZE, initial_capacity);
    _data = (unsigned char*) malloc(_capacity);
}

ProtoBuffer::~ProtoBuffer() {
    free(_data);
}

size_t ProtoBuffer::varIntSize(u64 value) {
    // size_varint = ceil(size_in_bits(value) / 7)
    // => size_varint = ceil[(64 - __builtin_clzll(value | 1)) / 7]
    // but ceil[N / 7] = floor[(N + 6) / 7
    // => size = (70 - __builtin_clzll(value | 1)) / 7
    // and N / 7 ≈ (N + 1) * 9 / 64 (for 0 <= N <= 63) gives the final formula
    // value | 1 is needed, __builtin_clzll not defined for 0
    return (640 - __builtin_clzll(value | 1) * 9) / 64;
}

void ProtoBuffer::ensureCapacity(size_t new_data_size) {
    size_t expected_capacity = _offset + new_data_size;
    if (expected_capacity <= _capacity) return;
    _capacity = MAX(expected_capacity, _capacity * 2);
    _data = (unsigned char*) realloc(_data, _capacity);
}

void ProtoBuffer::putVarInt(u64 n) {
    _offset = putVarInt(_offset, n);
}

size_t ProtoBuffer::putVarInt(size_t offset, u64 n) {
    ensureCapacity(varIntSize(n));
    while ((n >> 7) != 0) {
        _data[offset++] = (unsigned char) (0x80 | (n & 0x7f));
        n >>= 7;
    }
    _data[offset++] = (unsigned char) n;
    return offset;
}

void ProtoBuffer::tag(protobuf_index_t index, protobuf_t type) {
    putVarInt((u64) (index << 3 | type));
}

void ProtoBuffer::field(protobuf_index_t index, bool b) {
    field(index, (u64) b);
}

void ProtoBuffer::field(protobuf_index_t index, u32 n) {
    tag(index, VARINT);
    putVarInt(n);
}

void ProtoBuffer::field(protobuf_index_t index, u64 n) {
    tag(index, VARINT);
    putVarInt(n);
}

void ProtoBuffer::field(protobuf_index_t index, const char* s) {
    field(index, s, strlen(s));
}

void ProtoBuffer::field(protobuf_index_t index, const char* s, size_t len) {
    field(index, (const unsigned char*) s, strlen(s));
}

void ProtoBuffer::field(protobuf_index_t index, const unsigned char* s, size_t len) {
    tag(index, LEN);
    putVarInt((u64) len);

    ensureCapacity(len);
    memcpy(_data + _offset, s, len);
    _offset += len;
}

protobuf_mark_t ProtoBuffer::startMessage(protobuf_index_t index) {
    tag(index, LEN);

    ensureCapacity(NESTED_FIELD_BYTE_COUNT);
    _offset += NESTED_FIELD_BYTE_COUNT;
    return _offset;
}

void ProtoBuffer::commitMessage(protobuf_mark_t mark) {
    size_t message_length = _offset - mark;
    for (size_t i = 0; i < NESTED_FIELD_BYTE_COUNT - 1; ++i) {
        size_t idx = mark - NESTED_FIELD_BYTE_COUNT + i;
        _data[idx] = (unsigned char) (0x80 | (message_length & 0x7f));
        message_length >>= 7;
    }
    _data[mark - 1] = (unsigned char) message_length;
}
