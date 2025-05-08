/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BUFFER_H
#define _BUFFER_H

#include "os.h"
#include <arpa/inet.h>
#include <cstring>
#include <cstdint>

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

    std::size_t advanceOffset(std::size_t incr) {
        return _offset += incr;
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
        while (v > 0x7f) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
        }
        _data[_offset++] = (char)v;
    }

    void putVar32(std::size_t offset, u32 v) {
        _data[offset] = v | 0x80;
        _data[offset + 1] = (v >> 7) | 0x80;
        _data[offset + 2] = (v >> 14) | 0x80;
        _data[offset + 3] = (v >> 21) | 0x80;
        _data[offset + 4] = (v >> 28);
    }

    void putVar64(u64 v) {
        int iter = 0;
        while (v > 0x1fffff) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
            if (++iter == 3) return;
        }
        while (v > 0x7f) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
        }
        _data[_offset++] = (char)v;
    }

    void put(const char* v, std::size_t len) {
        std::memcpy(_data + _offset, v, len);
        _offset += len;
    }
};

#endif // _BUFFER_H
