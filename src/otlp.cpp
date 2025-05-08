/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "buffer.h"

class ProtobufBuffer : Buffer {
  public:
    ProtobufBuffer(char* data) : Buffer(data) {}

    void tag(int index, int type) {
        put8(index << 3 | type);
    }

    void field(int index, int n) {
        tag(index, 0);
        put32(n);
    }

    void field(int index, u32 n) {
        tag(index, 0);
        putVar32(n);
    }

    void field(int index, long n) {
        tag(index, 0);
        put64(n);
    }

    void field(int index, u64 n) {
        tag(index, 0);
        putVar64(n);
    }

    void field(int index, float n) {
        tag(index, 1);
        putFloat(n);
    }

    void field(int index, double n) {
        tag(index, 1);
        putFloat(n);
    }

    void field(int index, const char* s) {
        tag(index, 2);
        putUtf8(s);
    }

    void field(int index, const char* s, size_t len) {
        tag(index, 2);
        putByteString(s, len);
    }

    void field(int index, const Buffer buffer, size_t len) {
        field(index, data(), offset());
    }

    std::size_t startField(int index) {
        tag(index, 2);
        return advanceOffset(3);
    }

    void commitField(std::size_t mark) {
        std::size_t length = offset() - mark;
        set(0x80 | (length & 0x7f), mark - 3);
        set(0x80 | (length >> 7) & 0x7f, mark - 2);
        set(length >> 14, mark - 1);
    }
};