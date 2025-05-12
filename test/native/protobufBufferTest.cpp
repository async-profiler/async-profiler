/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf.h"
#include "testRunner.hpp"
#include <cstdint>
#include <string.h>

TEST_CASE(Buffer_test_var32_0) {
  char data[100];
  ProtobufBuffer buf(data);

  buf.field(3, (u32)0);

  CHECK_EQ(buf.offset(), 2);
  CHECK_EQ((unsigned char)buf.data()[0], (3 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[1], 0);
}

TEST_CASE(Buffer_test_var32_150) {
  char data[100];
  ProtobufBuffer buf(data);

  buf.field(3, (u32)150);

  CHECK_EQ(buf.offset(), 3);
  CHECK_EQ((unsigned char)buf.data()[0], (3 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[1], 150);
  CHECK_EQ((unsigned char)buf.data()[2], 1);
}

TEST_CASE(Buffer_test_var64_LargeNumber) {
  char data[100];
  ProtobufBuffer buf(data);

  buf.field(1, (u64)17574838338834838);

  CHECK_EQ(buf.offset(), 9);
  CHECK_EQ((unsigned char)buf.data()[0], (1 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[1], 150);
  CHECK_EQ((unsigned char)buf.data()[2], 163);
  CHECK_EQ((unsigned char)buf.data()[3], 175);
  CHECK_EQ((unsigned char)buf.data()[4], 225);
  CHECK_EQ((unsigned char)buf.data()[5], 142);
  CHECK_EQ((unsigned char)buf.data()[6], 135);
  CHECK_EQ((unsigned char)buf.data()[7], 156);
  CHECK_EQ((unsigned char)buf.data()[8], 31);
}

TEST_CASE(Buffer_test_var32_bool) {
  char data[100];
  ProtobufBuffer buf(data);

  buf.field(3, false);
  buf.field(4, true);
  buf.field(5, true);

  CHECK_EQ(buf.offset(), 6);
  CHECK_EQ((unsigned char)buf.data()[0], (3 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[1], 0);
  CHECK_EQ((unsigned char)buf.data()[2], (4 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[3], 1);
  CHECK_EQ((unsigned char)buf.data()[4], (5 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[5], 1);
}

TEST_CASE(Buffer_test_repeated_u32) {
  char data[100];
  ProtobufBuffer buf(data);

  buf.field(1, (u32) 34);
  buf.field(1, (u32) 28);

  CHECK_EQ(buf.offset(), 4);
  CHECK_EQ((unsigned char)buf.data()[0], (1 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[1], 34);
  CHECK_EQ((unsigned char)buf.data()[2], (1 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[3], 28);
}

TEST_CASE(Buffer_test_string) {
  char data[100];
  ProtobufBuffer buf(data);

  buf.field(4, "ciao");

  CHECK_EQ(buf.offset(), 6);
  CHECK_EQ((unsigned char)buf.data()[0], (4 << 3) | LEN);
  CHECK_EQ((unsigned char)buf.data()[1], 4);
  CHECK_EQ(strncmp(buf.data() + 2, "ciao", 4), 0);
}

TEST_CASE(Buffer_test_nestedField) {
  char data[100];
  ProtobufBuffer buf(data);

  {
    ProtobufBuffer childMessage = buf.startMessage(4);
    childMessage.field(3, (u32)10);
    childMessage.field(5, true);
  }

  CHECK_EQ(buf.offset(), 8);
  CHECK_EQ((unsigned char)buf.data()[0], (4 << 3) | LEN);
  CHECK_EQ((unsigned char)buf.data()[1], 4 | 0b10000000); // Length of the field with continuation bit as MSB
  CHECK_EQ((unsigned char)buf.data()[2], 0b10000000);     // Continuation bit MSB
  CHECK_EQ((unsigned char)buf.data()[3], 0);              // Continuation bit MSB
  CHECK_EQ((unsigned char)buf.data()[4], (3 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[5], 10);
  CHECK_EQ((unsigned char)buf.data()[6], (5 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[7], 1);
}

TEST_CASE(Buffer_test_nestedMessageWithString) {
    char data[100];
    ProtobufBuffer buf(data);

    {
    ProtobufBuffer nested1 = buf.startMessage(3);
    ProtobufBuffer nested2 = nested1.startMessage(4);
    nested2.field(5, "ciao");
    }

    CHECK_EQ(buf.offset(), (1 + 3) + (1 + 3 + 1 + 1 + 4));
    CHECK_EQ((unsigned char)buf.data()[0], (3 << 3) | LEN);
    CHECK_EQ((unsigned char)buf.data()[1], 10 | 0b10000000);
    CHECK_EQ((unsigned char)buf.data()[2], 0b10000000);
    CHECK_EQ((unsigned char)buf.data()[3], 0);
    CHECK_EQ((unsigned char)buf.data()[4], (4 << 3) | LEN);
    CHECK_EQ((unsigned char)buf.data()[5], 6 | 0b10000000);
    CHECK_EQ((unsigned char)buf.data()[6], 0b10000000);
    CHECK_EQ((unsigned char)buf.data()[7], 0);
    CHECK_EQ((unsigned char)buf.data()[8], (5 << 3) | LEN);
    CHECK_EQ((unsigned char)buf.data()[9], 4);
    CHECK_EQ(strncmp(buf.data() + 10, "ciao", 4), 0);
}

TEST_CASE(Buffer_test_maxTag) {
    char data[100];
    ProtobufBuffer buf(data);

    // https://protobuf.dev/programming-guides/proto3/#assigning-field-numbers
    const u32 max_tag = 536870911;
    buf.field(max_tag, (u32) 3);

    CHECK_EQ(buf.offset(), 6);
    // Check the value of the first 5 bytes as a varint
    u32 sum = (unsigned char) buf.data()[5] & 0b01111111;
    for (int idx = 4; idx >= 0; --idx) {
        sum <<= 7;
        sum += ((unsigned char) buf.data()[idx] & 0b01111111);
    }
    CHECK_EQ(sum, (max_tag << 3) | VARINT);
    CHECK_EQ((unsigned char)buf.data()[5], 3);

}
