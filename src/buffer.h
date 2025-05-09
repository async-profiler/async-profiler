/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BUFFER_H
#define _BUFFER_H

#include "os.h"
#include <arpa/inet.h>
#include <cstdint>
#include <string.h>

class Buffer {
  protected:
    std::size_t _offset;
    char _data[0];

    Buffer() : _offset(0) {
    }

  public:
    const char* data() const {
        return _data;
    }

    std::size_t offset() const {
        return _offset;
    }

    int skip(std::size_t delta) {
        std::size_t offset = _offset;
        _offset = offset + delta;
        return offset;
    }

    void reset() {
        _offset = 0;
    }

    void put8(char v) {
        _data[_offset++] = v;
    }

    void put8(std::size_t offset, char v) {
        _data[offset] = v;
    }

    virtual void put16(short v) = 0;
    virtual void put32(int v) = 0;
    virtual void put64(u64 v) = 0;

    void putFloat(float v) {
        union {
            float f;
            int i;
        } u;

        u.f = v;
        put32(u.i);
    }

    void putDouble(double v) {
        union {
            double d;
            long l;
        } u;

        u.d = v;
        put64(u.l);
    }

    void put(const char* v, std::size_t len) {
        memcpy(_data + _offset, v, len);
        _offset += len;
    }
};

class BigEndianBuffer : public Buffer {
  public:
    void put16(short v) override {
        *(short*)(_data + _offset) = htons(v);
        _offset += 2;
    }

    void put32(int v) override {
        *(int*)(_data + _offset) = htonl(v);
        _offset += 4;
    }

    void put64(u64 v) override {
        *(u64*)(_data + _offset) = OS::hton64(v);
        _offset += 8;
    }
};

inline bool is_system_little_endian() {
    const int value = 0x01;
    const void * address = static_cast<const void *>(&value);
    const unsigned char * least_significant_address = static_cast<const unsigned char *>(address);
    return *least_significant_address == 0x01;
}

class LittleEndianBuffer : public Buffer {
  public:
    void put16(short v) override {
        if (is_system_little_endian()) {
            *(short*)(_data + _offset) = v;
        } else {
            *(short*)(_data + _offset) = __builtin_bswap16(v);
        }
        _offset += 2;
    }

    void put32(int v) override {
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

#endif // _BUFFER_H
