/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>

constexpr int MAX_STRING_LENGTH = 8191;

class Buffer {
  private:
    size_t _offset;
    char _data[0];

  protected:
    Buffer() : _offset(0) {
    }

  public:
    const char* data() const {
        return _data;
    }

    int offset() const {
        return _offset;
    }

    int skip(int delta) {
        int offset = _offset;
        _offset = offset + delta;
        return offset;
    }

    void reset() {
        _offset = 0;
    }

    void put(const char* v, u32 len) {
        memcpy(_data + _offset, v, len);
        _offset += (int)len;
    }

    void put8(char v) {
        _data[_offset++] = v;
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

    void putVar32(u32 v) {
        while (v > 0x7f) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
        }
        _data[_offset++] = (char)v;
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

    void putUtf8(const char* v) {
        if (v == NULL) {
            put8(0);
        } else {
            size_t len = strlen(v);
            putUtf8(v, len < MAX_STRING_LENGTH ? len : MAX_STRING_LENGTH);
        }
    }

    void putUtf8(const char* v, u32 len) {
        put8(3);
        putVar32(len);
        put(v, len);
    }

    void putByteString(const char* v, u32 len) {
        put8(5); // STRING_ENCODING_LATIN1_BYTE_ARRAY
        putVar32(len);
        put(v, len);
    }

    void put8(int offset, char v) {
        _data[offset] = v;
    }

    void putVar32(int offset, u32 v) {
        _data[offset] = v | 0x80;
        _data[offset + 1] = (v >> 7) | 0x80;
        _data[offset + 2] = (v >> 14) | 0x80;
        _data[offset + 3] = (v >> 21) | 0x80;
        _data[offset + 4] = (v >> 28);
    }
};
