/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "protobuf.h"
#include "testRunner.hpp"
#include <cstdint>
#include <string.h>

TEST_CASE(Buffer_test_var32_0) {
  char *data = (char *)alloca(100);
  ProtobufBuffer buf(data);

  buf.field(3, (u32)0);

  CHECK_EQ(buf.offset(), 2);
  CHECK_EQ((unsigned char)buf.data()[0], (3 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[1], 0);
}

TEST_CASE(Buffer_test_var32_150) {
  char *data = (char *)alloca(100);
  ProtobufBuffer buf(data);

  buf.field(3, (u32)150);

  CHECK_EQ(buf.offset(), 3);
  CHECK_EQ((unsigned char)buf.data()[0], (3 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[1], 150);
  CHECK_EQ((unsigned char)buf.data()[2], 1);
}

TEST_CASE(Buffer_test_var64_LargeNumber) {
  char *data = (char *)alloca(100);
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
  char *data = (char *)alloca(100);
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

TEST_CASE(Buffer_test_repeated_int32) {
  char *data = (char *)alloca(100);
  ProtobufBuffer buf(data);

  buf.field(1, 34);
  buf.field(1, 28);

  CHECK_EQ(buf.offset(), 10);
  CHECK_EQ((unsigned char)buf.data()[0], (1 << 3) | I32);
  CHECK_EQ((unsigned char)buf.data()[1], 34);
  CHECK_EQ((unsigned char)buf.data()[2], 0);
  CHECK_EQ((unsigned char)buf.data()[3], 0);
  CHECK_EQ((unsigned char)buf.data()[4], 0);
  CHECK_EQ((unsigned char)buf.data()[5], (1 << 3) | I32);
  CHECK_EQ((unsigned char)buf.data()[6], 28);
  CHECK_EQ((unsigned char)buf.data()[7], 0);
  CHECK_EQ((unsigned char)buf.data()[8], 0);
  CHECK_EQ((unsigned char)buf.data()[9], 0);
}

TEST_CASE(Buffer_test_repeated_double) {
  char *data = (char *)alloca(100);
  ProtobufBuffer buf(data);

  buf.field(2, 34.1);
  buf.field(2, 28.2);

  CHECK_EQ(buf.offset(), 18);
  CHECK_EQ((unsigned char)buf.data()[0], (2 << 3) | I64);
  CHECK_EQ((unsigned char)buf.data()[1], 205);
  CHECK_EQ((unsigned char)buf.data()[2], 204);
  CHECK_EQ((unsigned char)buf.data()[3], 204);
  CHECK_EQ((unsigned char)buf.data()[4], 204);
  CHECK_EQ((unsigned char)buf.data()[5], 204);
  CHECK_EQ((unsigned char)buf.data()[6], 12);
  CHECK_EQ((unsigned char)buf.data()[7], 65);
  CHECK_EQ((unsigned char)buf.data()[8], 64);
  CHECK_EQ((unsigned char)buf.data()[9], (2 << 3) | I64);
  CHECK_EQ((unsigned char)buf.data()[10], 51);
  CHECK_EQ((unsigned char)buf.data()[11], 51);
  CHECK_EQ((unsigned char)buf.data()[12], 51);
  CHECK_EQ((unsigned char)buf.data()[13], 51);
  CHECK_EQ((unsigned char)buf.data()[14], 51);
  CHECK_EQ((unsigned char)buf.data()[15], 51);
  CHECK_EQ((unsigned char)buf.data()[16], 60);
  CHECK_EQ((unsigned char)buf.data()[17], 64);
}

TEST_CASE(Buffer_test_string) {
  char *data = (char *)alloca(100);
  ProtobufBuffer buf(data);

  buf.field(4, "ciao");

  CHECK_EQ(buf.offset(), 6);
  CHECK_EQ((unsigned char)buf.data()[0], (4 << 3) | LEN);
  CHECK_EQ((unsigned char)buf.data()[1], 4);
  CHECK_EQ(strncmp(buf.data() + 2, "ciao", 4), 0);
}

TEST_CASE(Buffer_test_nestedField) {
  char *data = (char *)alloca(100);
  ProtobufBuffer buf(data);

  protobuf_field_mark_t mark = buf.startMessage(4);
  buf.field(3, (u32)10);
  buf.field(5, true);
  buf.commitMessage(mark);

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

TEST_CASE(Buffer_test_map) {
  char *data = (char *)alloca(100);
  ProtobufBuffer buf(data);

  buf.mapEntry<>(5, "one", (u32)1);
  buf.mapEntry<>(5, "two", (u32)2);
  buf.mapEntry<>(5, "three", (u32)3);

  const int msg1_size = 1 + 3 + 2 + strlen("one") + 2;
  const int msg2_size = 1 + 3 + 2 + strlen("two") + 2;
  const int msg3_size = 1 + 3 + 2 + strlen("three") + 2;

  CHECK_EQ(buf.offset(), msg1_size + msg2_size + msg3_size);

  CHECK_EQ((unsigned char)buf.data()[0], (5 << 3) | LEN);
  // msg length
  CHECK_EQ((unsigned char)buf.data()[1], 7 | 0b10000000);
  CHECK_EQ((unsigned char)buf.data()[2], 0b10000000);
  CHECK_EQ((unsigned char)buf.data()[3], 0);
  // key
  CHECK_EQ((unsigned char)buf.data()[4], (1 << 3) | LEN);
  CHECK_EQ((unsigned char)buf.data()[5], 3);
  CHECK_EQ(strncmp(buf.data() + 6, "one", 3), 0);
  // value
  CHECK_EQ((unsigned char)buf.data()[9], (2 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[10], 1);

  CHECK_EQ((unsigned char)buf.data()[11], (5 << 3) | LEN);
  // msg length
  CHECK_EQ((unsigned char)buf.data()[12], 7 | 0b10000000);
  CHECK_EQ((unsigned char)buf.data()[13], 0b10000000);
  CHECK_EQ((unsigned char)buf.data()[14], 0);
  // key
  CHECK_EQ((unsigned char)buf.data()[15], (1 << 3) | LEN);
  CHECK_EQ((unsigned char)buf.data()[16], 3);
  CHECK_EQ(strncmp(buf.data() + 17, "two", 3), 0);
  // value
  CHECK_EQ((unsigned char)buf.data()[20], (2 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[21], 2);

  CHECK_EQ((unsigned char)buf.data()[22], (5 << 3) | LEN);
  // msg length
  CHECK_EQ((unsigned char)buf.data()[23], 9 | 0b10000000);
  CHECK_EQ((unsigned char)buf.data()[24], 0b10000000);
  CHECK_EQ((unsigned char)buf.data()[25], 0);
  // key
  CHECK_EQ((unsigned char)buf.data()[26], (1 << 3) | LEN);
  CHECK_EQ((unsigned char)buf.data()[27], 5);
  CHECK_EQ(strncmp(buf.data() + 28, "three", 5), 0);
  // value
  CHECK_EQ((unsigned char)buf.data()[33], (2 << 3) | VARINT);
  CHECK_EQ((unsigned char)buf.data()[34], 3);
}
