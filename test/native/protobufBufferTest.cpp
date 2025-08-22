/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf.h"
#include "testRunner.hpp"
#include <string.h>

static u64 readVarInt(const unsigned char* data) {
    size_t idx = 0;
    u64 output = 0;
    while (data[idx] & 0x80) {
        output += ((u64) data[idx] & 0x7F) << (7 * idx);
        idx++;
    }
    return output + ((u64) data[idx] << (7 * idx));
}

TEST_CASE(Buffer_test_varint_0) {
    ProtoBuffer buf(100);

    buf.field(3, (u64)0);

    CHECK_EQ(buf.offset(), 2);
    CHECK_EQ(buf.data()[0], (3 << 3) | VARINT);
    CHECK_EQ(readVarInt(buf.data() + 1), 0);
}

TEST_CASE(Buffer_test_varint_150) {
    ProtoBuffer buf(100);

    buf.field(3, (u64)150);

    CHECK_EQ(buf.offset(), 3);
    CHECK_EQ(buf.data()[0], (3 << 3) | VARINT);
    CHECK_EQ(readVarInt(buf.data() + 1), 150);
}

TEST_CASE(Buffer_test_varint_LargeNumber) {
    ProtoBuffer buf(100);

    buf.field(1, (u64)17574838338834838);

    CHECK_EQ(buf.offset(), 9);
    CHECK_EQ(buf.data()[0], (1 << 3) | VARINT);
    CHECK_EQ(readVarInt(buf.data() + 1), 17574838338834838);
}

TEST_CASE(Buffer_test_var32_bool) {
    ProtoBuffer buf(100);

    buf.field(3, false);
    buf.field(4, true);
    buf.field(5, true);

    CHECK_EQ(buf.offset(), 6);
    CHECK_EQ(buf.data()[0], (3 << 3) | VARINT);
    CHECK_EQ(buf.data()[1], 0);
    CHECK_EQ(buf.data()[2], (4 << 3) | VARINT);
    CHECK_EQ(buf.data()[3], 1);
    CHECK_EQ(buf.data()[4], (5 << 3) | VARINT);
    CHECK_EQ(buf.data()[5], 1);
}

TEST_CASE(Buffer_test_repeated_u64) {
    ProtoBuffer buf(100);

    buf.field(1, (u64) 34);
    buf.field(1, (u64) 28);

    CHECK_EQ(buf.offset(), 4);
    CHECK_EQ(buf.data()[0], (1 << 3) | VARINT);
    CHECK_EQ(buf.data()[1], 34);
    CHECK_EQ(buf.data()[2], (1 << 3) | VARINT);
    CHECK_EQ(buf.data()[3], 28);
}

TEST_CASE(Buffer_test_string) {
    ProtoBuffer buf(100);

    buf.field(4, "ciao");

    CHECK_EQ(buf.offset(), 6);
    CHECK_EQ(buf.data()[0], (4 << 3) | LEN);
    CHECK_EQ(buf.data()[1], 4);
    CHECK_EQ(strncmp((const char*) buf.data() + 2, "ciao", 4), 0);
}

TEST_CASE(Buffer_test_nestedField) {
    ProtoBuffer buf(100);

    protobuf_mark_t mark = buf.startMessage(4, 3);
    buf.field(3, (u64)10);
    buf.field(5, true);
    buf.commitMessage(mark);

    CHECK_EQ(buf.offset(), 8);
    CHECK_EQ(buf.data()[0], (4 << 3) | LEN);
    CHECK_EQ(buf.data()[1], 4 | 0x80); // Length of the field with continuation bit as MSB
    CHECK_EQ(buf.data()[2], 0x80);     // Continuation bit MSB
    CHECK_EQ(buf.data()[3], 0);        // Continuation bit MSB
    CHECK_EQ(buf.data()[4], (3 << 3) | VARINT);
    CHECK_EQ(buf.data()[5], 10);
    CHECK_EQ(buf.data()[6], (5 << 3) | VARINT);
    CHECK_EQ(buf.data()[7], 1);
}

TEST_CASE(Buffer_test_nestedMessageWithString) {
    ProtoBuffer buf(100);

    protobuf_mark_t mark1 = buf.startMessage(3, 3);
    protobuf_mark_t mark2 = buf.startMessage(4, 3);
    buf.field(5, "ciao");
    buf.commitMessage(mark1);
    buf.commitMessage(mark2);

    CHECK_EQ(buf.offset(), (1 + 3) + (1 + 3 + 1 + 1 + 4));
    CHECK_EQ(buf.data()[0], (3 << 3) | LEN);
    CHECK_EQ(buf.data()[1], 10 | 0x80);
    CHECK_EQ(buf.data()[2], 0x80);
    CHECK_EQ(buf.data()[3], 0);
    CHECK_EQ(buf.data()[4], (4 << 3) | LEN);
    CHECK_EQ(buf.data()[5], 6 | 0x80);
    CHECK_EQ(buf.data()[6], 0x80);
    CHECK_EQ(buf.data()[7], 0);
    CHECK_EQ(buf.data()[8], (5 << 3) | LEN);
    CHECK_EQ(buf.data()[9], 4);
    CHECK_EQ(strncmp((const char*) buf.data() + 10, "ciao", 4), 0);
}

TEST_CASE(Buffer_test_maxTag) {
    ProtoBuffer buf(100);

    // https://protobuf.dev/programming-guides/proto3/#assigning-field-numbers
    const protobuf_index_t max_tag = 536870911;
    buf.field(max_tag, (u64) 3);

    CHECK_EQ(buf.offset(), 6);
    // Check the value of the first 5 bytes as a varint
    u32 sum = buf.data()[5] & 0x7f;
    for (int idx = 4; idx >= 0; --idx) {
        sum <<= 7;
        sum += (buf.data()[idx] & 0x7f);
    }
    CHECK_EQ(sum, (max_tag << 3) | VARINT);
    CHECK_EQ(buf.data()[5], 3);
}

TEST_CASE(Buffer_test_relocation) {
    ProtoBuffer buf(0);
    CHECK_EQ(buf.capacity(), 16);
    CHECK_EQ(buf.offset(), 0);

    for (int i = 0; i < 8; ++i) buf.field(2, (u64) i);
    CHECK_EQ(buf.capacity(), 16);
    CHECK_EQ(buf.offset(), 16);

    buf.field(4, "abc");
    CHECK_EQ(buf.capacity(), 32);
    CHECK_EQ(buf.offset(), 21);

    // check everything was relocated properly
    for (int i = 0; i < 8; ++i) {
        CHECK_EQ(buf.data()[i*2], (2 << 3) | VARINT);
        CHECK_EQ(buf.data()[i*2+1], i);
    }
    CHECK_EQ(buf.data()[16], (4 << 3) | LEN);
    CHECK_EQ(buf.data()[17], 3);
    CHECK_EQ(strncmp((const char*) buf.data() + 18, "abc", 3), 0);
}

TEST_CASE(Buffer_test_VarIntByteSize) {
    CHECK_EQ(ProtoBuffer::varIntSize(0), 1);
    CHECK_EQ(ProtoBuffer::varIntSize(0x7F), 1);
    CHECK_EQ(ProtoBuffer::varIntSize(0x3FFF), 2);
    CHECK_EQ(ProtoBuffer::varIntSize(0x1FFFFF), 3);
    CHECK_EQ(ProtoBuffer::varIntSize(0xFFFFFFF), 4);
    CHECK_EQ(ProtoBuffer::varIntSize(0x7FFFFFFFF), 5);
    CHECK_EQ(ProtoBuffer::varIntSize(0x3FFFFFFFFFF), 6);
    CHECK_EQ(ProtoBuffer::varIntSize(0x1FFFFFFFFFFFF), 7);
    CHECK_EQ(ProtoBuffer::varIntSize(0xFFFFFFFFFFFFFF), 8);
    CHECK_EQ(ProtoBuffer::varIntSize(0x7FFFFFFFFFFFFFFF), 9);
    CHECK_EQ(ProtoBuffer::varIntSize(0xFFFFFFFFFFFFFFFF), 10);
}

TEST_CASE(Buffer_test_string_with_explicit_length) {
    ProtoBuffer buf(100);
    
    const char* longString = "hello_world_this_is_a_long_string";
    size_t partialLength = 5;

    buf.field(1, longString, partialLength);
    
    CHECK_EQ(buf.offset(), 1 + 1 + partialLength);
    CHECK_EQ(buf.data()[0], (1 << 3) | LEN);
    CHECK_EQ(buf.data()[1], partialLength);
    CHECK_EQ(strncmp((const char*) buf.data() + 2, "hello", partialLength), 0);
    
}
