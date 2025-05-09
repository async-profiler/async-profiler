/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OTLP_H
#define _OTLP_H

#include "arch.h"
#include "buffer.h"
#include <type_traits>

typedef const u8 protobuf_t;
static protobuf_t VARINT = 0;
static protobuf_t I64 = 1;
static protobuf_t LEN = 2;
static protobuf_t I32 = 5;

inline bool is_system_little_endian() {
    const int value = 0x01;
    const void* address = static_cast<const void*>(&value);
    const unsigned char* least_significant_address = static_cast<const unsigned char*>(address);
    return *least_significant_address == 0x01;
}

class LittleEndianBuffer : public Buffer {
  public:
    LittleEndianBuffer(char* data) : Buffer(data) {}

    void put16(u16 v) override {
        if (is_system_little_endian()) {
            *(short*)(_data + _offset) = v;
        } else {
            *(short*)(_data + _offset) = __builtin_bswap16(v);
        }
        _offset += 2;
    }

    void put32(u32 v) override {
        if (is_system_little_endian()) {
            *(int*)(_data + _offset) = v;
        } else {
            *(int*)(_data + _offset) = __builtin_bswap32(v);
        }
        _offset += 4;
    }

    void put64(u64 v) override {
        if (is_system_little_endian()) {
            *(long*)(_data + _offset) = v;
        } else {
            *(long*)(_data + _offset) = __builtin_bswap64(v);
        }
        _offset += 8;
    }
};

class ProtobufBuffer : public LittleEndianBuffer {
  private:
    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, void>::type putVarInt(T n);

    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, std::size_t>::type putVarInt(std::size_t offset, T n);

    void tag(int index, protobuf_t type);

  public:
    ProtobufBuffer(char* data) : LittleEndianBuffer(data) {}

    void field(int index, int n);
    void field(int index, u32 n);
    void field(int index, long n);
    void field(int index, u64 n);
    void field(int index, float n);
    void field(int index, double n);
    void field(int index, const char* s);
    void field(int index, const char* s, size_t len);
    void field(int index, const LittleEndianBuffer buffer, size_t len);

    std::size_t startField(int index);
    void commitField(std::size_t mark);
};

#endif // _OTLP_H
