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
  private:
    std::size_t _offset;
    char _data[0];

  protected:
    Buffer() : _offset(0) {
    }

    void set(char v, std::size_t idx) {
        _data[idx] = v;
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

    void put16(short v) {
        *(short*)(_data + _offset) = htons(v);
        _offset += 2;
    }

    void put32(int v) {
        *(int*)(_data + _offset) = htonl(v);
        _offset += 4;
    }

    void put64(u64 v) {
        *(u64*)(_data + _offset) = OS::hton64(v);
        _offset += 8;
    }

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

    void putVar32(u32 v) {
        _offset = putVar32(_offset, v);
    }

    std::size_t putVar32(std::size_t offset, u32 v) {
        while (v > 0b01111111) {
            _data[offset++] = (char)v | 0b10000000;
            v >>= 7;
        }
        _data[offset++] = (char)v;
        return offset;
    }

    void putVar64(u64 v) {
        _offset = putVar64(_offset, v);
    }

    std::size_t putVar64(std::size_t offset, u64 v) {
        int iter = 0;
        while (v > 0b111111111111111111111) {
            _data[offset++] = (char)v | 0b10000000;
            v >>= 7;
        }
        while (v > 0b01111111) {
            _data[offset++] = (char)v | 0b10000000;
            v >>= 7;
        }
        if (v > 0) {
            _data[offset++] = (char)v;
        }
        return offset;
    }

    void put(const char* v, std::size_t len) {
        memcpy(_data + _offset, v, len);
        _offset += len;
    }
};

#endif // _BUFFER_H
