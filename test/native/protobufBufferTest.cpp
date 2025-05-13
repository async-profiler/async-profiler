/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf.h"
#include "testRunner.hpp"
#include <cstdint>
#include <string.h>

TEST_CASE(Buffer_test_var32_0) {
  ProtobufBuffer buf(100);

  buf.field(3, (u64)0);

  CHECK_EQ(buf.offset(), 2);
  CHECK_EQ(buf.data()[0], (3 << 3) | VARINT);
  CHECK_EQ(buf.data()[1], 0);
}

TEST_CASE(Buffer_test_var32_150) {
  ProtobufBuffer buf(100);

  buf.field(3, (u64)150);

  CHECK_EQ(buf.offset(), 3);
  CHECK_EQ(buf.data()[0], (3 << 3) | VARINT);
  CHECK_EQ(buf.data()[1], 150);
  CHECK_EQ(buf.data()[2], 1);
}

TEST_CASE(Buffer_test_var64_LargeNumber) {
  ProtobufBuffer buf(100);

  buf.field(1, (u64)17574838338834838);

  CHECK_EQ(buf.offset(), 9);
  CHECK_EQ(buf.data()[0], (1 << 3) | VARINT);
  CHECK_EQ(buf.data()[1], 150);
  CHECK_EQ(buf.data()[2], 163);
  CHECK_EQ(buf.data()[3], 175);
  CHECK_EQ(buf.data()[4], 225);
  CHECK_EQ(buf.data()[5], 142);
  CHECK_EQ(buf.data()[6], 135);
  CHECK_EQ(buf.data()[7], 156);
  CHECK_EQ(buf.data()[8], 31);
}

TEST_CASE(Buffer_test_var32_bool) {
  ProtobufBuffer buf(100);

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
  ProtobufBuffer buf(100);

  buf.field(1, (u64) 34);
  buf.field(1, (u64) 28);

  CHECK_EQ(buf.offset(), 4);
  CHECK_EQ(buf.data()[0], (1 << 3) | VARINT);
  CHECK_EQ(buf.data()[1], 34);
  CHECK_EQ(buf.data()[2], (1 << 3) | VARINT);
  CHECK_EQ(buf.data()[3], 28);
}

TEST_CASE(Buffer_test_string) {
  ProtobufBuffer buf(100);

  buf.field(4, "ciao");

  CHECK_EQ(buf.offset(), 6);
  CHECK_EQ(buf.data()[0], (4 << 3) | LEN);
  CHECK_EQ(buf.data()[1], 4);
  CHECK_EQ(strncmp((const char*) buf.data() + 2, "ciao", 4), 0);
}

TEST_CASE(Buffer_test_nestedField) {
  ProtobufBuffer buf(100);

  protobuf_mark_t mark = buf.startMessage(4);
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
  ProtobufBuffer buf(100);

  protobuf_mark_t mark1 = buf.startMessage(3);
  protobuf_mark_t mark2 = buf.startMessage(4);
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
  ProtobufBuffer buf(100);

  // https://protobuf.dev/programming-guides/proto3/#assigning-field-numbers
  const protobuf_mark_t max_tag = 536870911;
  buf.field(max_tag, (u64) 3);

  CHECK_EQ(buf.offset(), 6);
  // Check the value of the first 5 bytes as a varint
  u32 sum = buf.data()[5] & 0x7f;
  for (int idx = 4; idx >= 0; --idx) {
      sum <<= 7;
      sum += ( buf.data()[idx] & 0x7f);
  }
  CHECK_EQ(sum, (max_tag << 3) | VARINT);
  CHECK_EQ(buf.data()[5], 3);
}
