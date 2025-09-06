/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/param.h>
#include <string.h>
#include <stdlib.h>
#include "assert.h"
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
    ensureCapacity(varIntSize(n));
    while ((n >> 7) != 0) {
        _data[_offset++] = (unsigned char) (0x80 | n);
        n >>= 7;
    }
    _data[_offset++] = (unsigned char) n;
}

void ProtoBuffer::tag(protobuf_index_t index, protobuf_t type) {
    putVarInt((u64) (index << 3 | type));
}

void ProtoBuffer::field(protobuf_index_t index, u64 n) {
    tag(index, VARINT);
    putVarInt(n);
}

void ProtoBuffer::field(protobuf_index_t index, const char* s) {
    field(index, s, strlen(s));
}

void ProtoBuffer::field(protobuf_index_t index, const char* s, size_t len) {
    field(index, (const unsigned char*) s, len);
}

void ProtoBuffer::field(protobuf_index_t index, const unsigned char* s, size_t len) {
    tag(index, LEN);
    putVarInt((u64) len);

    ensureCapacity(len);
    memcpy(_data + _offset, s, len);
    _offset += len;
}

protobuf_mark_t ProtoBuffer::startMessage(protobuf_index_t index, size_t max_len_byte_count) {
    tag(index, LEN);

    ensureCapacity(max_len_byte_count);

    protobuf_mark_t mark = _offset << 3 | max_len_byte_count;
    _offset += max_len_byte_count;
    return mark;
}

void ProtoBuffer::commitMessage(protobuf_mark_t mark) {
    size_t max_len_byte_count = mark & 7;
    size_t message_start = mark >> 3;

    size_t actual_len = _offset - (message_start + max_len_byte_count);
    assert(varIntSize(actual_len) <= max_len_byte_count);

    for (size_t i = 0; i < max_len_byte_count - 1; i++) {
        _data[message_start + i] = (unsigned char) (0x80 | actual_len);
        actual_len >>= 7;
    }
    _data[message_start + max_len_byte_count - 1] = (unsigned char) actual_len;
}
