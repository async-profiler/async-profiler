/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "buffer.h"
#include <type_traits>

class ProtobufBuffer : Buffer {
  private:
    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, void>::type putVarInt(T n) {
        _offset = putVarInt(_offset, n);
    }

    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, std::size_t>::type putVarInt(std::size_t offset, T n) {
        while ((n >> 7) != 0) {
            _data[offset++] = (char)(0b10000000 | (n & 0b01111111));
            n >>= 7;
        }
        _data[offset++] = (char)n;
        return offset;
    }

  public:
    ProtobufBuffer(char* data) : Buffer() {}

    void tag(int index, int type) {
        put8(index << 3 | type);
    }

    void field(int index, int n) {
        tag(index, 0);
        put32(n);
    }

    void field(int index, u32 n) {
        tag(index, 0);
        putVarInt<>(n);
    }

    void field(int index, long n) {
        tag(index, 0);
        put64(n);
    }

    void field(int index, u64 n) {
        tag(index, 0);
        putVarInt<>(n);
    }

    void field(int index, float n) {
        tag(index, 1);
        putFloat(n);
    }

    void field(int index, double n) {
        tag(index, 1);
        putDouble(n);
    }

    void field(int index, const char* s) {
        field(index, s, strlen(s));
    }

    void field(int index, const char* s, size_t len) {
        tag(index, 2);
        put(s, len);
    }

    void field(int index, const Buffer buffer, size_t len) {
        field(index, data(), offset());
    }

    std::size_t startField(int index) {
        tag(index, 2);
        skip(3);
        return offset();
    }

    void commitField(std::size_t mark) {
        std::size_t length = offset() - mark;
        putVarInt<>(mark - 3, length);
    }
};
