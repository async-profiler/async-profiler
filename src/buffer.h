/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BUFFER_H
#define _BUFFER_H

#include "os.h"
#include <cstdint>
#include <string.h>

class Buffer {
  protected:
    size_t _offset;
    char* _data;

    Buffer(char* data) : _offset(0), _data(data) {
    }

  public:
    const char* data() const {
        return _data;
    }

    size_t offset() const {
        return _offset;
    }

    int skip(size_t delta) {
        size_t offset = _offset;
        _offset = offset + delta;
        return offset;
    }

    void reset() {
        _offset = 0;
    }

    void put8(char v) {
        _data[_offset++] = v;
    }

    void put8(size_t offset, char v) {
        _data[offset] = v;
    }

    virtual void put16(u16 v) = 0;
    virtual void put32(u32 v) = 0;
    virtual void put64(u64 v) = 0;

    void putFloat(float v) {
        union {
            float f;
            u32 i;
        } u;

        u.f = v;
        put32(u.i);
    }

    void putDouble(double v) {
        union {
            double d;
            u64 l;
        } u;

        u.d = v;
        put64(u.l);
    }

    void put(const char* v, size_t len) {
        memcpy(_data + _offset, v, len);
        _offset += len;
    }
};

#endif // _BUFFER_H
