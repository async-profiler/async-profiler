/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OTLP_H
#define _OTLP_H

#include "arch.h"
#include "os.h"
#include <string.h>
#include <type_traits>

typedef const u8 protobuf_t;
static protobuf_t VARINT = 0;
static protobuf_t I64 = 1;
static protobuf_t LEN = 2;
static protobuf_t I32 = 5;

// We assume the length of a nested field can be represented with 3 varint
// bytes.
// TODO: What if it doesn't? Do we need to account for this case?
const size_t nested_field_byte_count = 3;

static bool is_system_little_endian() {
  const int value = 0x01;
  const void *address = static_cast<const void *>(&value);
  const unsigned char *least_significant_address =
      static_cast<const unsigned char *>(address);
  return *least_significant_address == 0x01;
}

class LittleEndianBuffer {
private:
  const bool _is_little_endian;

protected:
  size_t _offset;
  char *_data;

  LittleEndianBuffer(char *data)
      : _offset(0), _data(data), _is_little_endian(is_system_little_endian()) {}

public:
  const char *data() const { return _data; }

  size_t offset() const { return _offset; }

  int skip(size_t delta) {
    size_t offset = _offset;
    _offset = offset + delta;
    return offset;
  }

  void reset() { _offset = 0; }

  void put8(char v) { _data[_offset++] = v; }

  void put8(size_t offset, char v) { _data[offset] = v; }

  void put16(u16 v) {
    if (_is_little_endian) {
      *(short *)(_data + _offset) = v;
    } else {
      *(short *)(_data + _offset) = __builtin_bswap16(v);
    }
    _offset += 2;
  }

  void put32(u32 v) {
    if (_is_little_endian) {
      *(int *)(_data + _offset) = v;
    } else {
      *(int *)(_data + _offset) = __builtin_bswap32(v);
    }
    _offset += 4;
  }

  void put64(u64 v) {
    if (_is_little_endian) {
      *(long *)(_data + _offset) = v;
    } else {
      *(long *)(_data + _offset) = __builtin_bswap64(v);
    }
    _offset += 8;
  }

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

  void put(const char *v, size_t len) {
    memcpy(_data + _offset, v, len);
    _offset += len;
  }
};

class ProtobufBuffer : private LittleEndianBuffer {
private:
  template <typename T>
  typename std::enable_if<std::is_unsigned<T>::value, void>::type
  putVarInt(T n);

  template <typename T>
  typename std::enable_if<std::is_unsigned<T>::value, size_t>::type
  putVarInt(size_t offset, T n);

  void tag(int index, protobuf_t type);

public:
  ProtobufBuffer(char *data) : LittleEndianBuffer(data) {}

  const char *data() const { return _data; }

  size_t offset() const { return _offset; }

  void field(int index, bool b);
  void field(int index, int n);
  void field(int index, u32 n);
  void field(int index, long n);
  void field(int index, u64 n);
  void field(int index, float n);
  void field(int index, double n);
  void field(int index, const char *s);
  void field(int index, const char *s, size_t len);
  void field(int index, const LittleEndianBuffer buffer, size_t len);

  size_t startField(int index);
  void commitField(size_t mark);

  template <typename K, typename V>
  void mapField(int mapIndex, K key, V value);
};

template <typename K, typename V>
void ProtobufBuffer::mapField(int mapIndex, K key, V value) {
  size_t mark = startField(mapIndex);
  field(1, key);
  field(2, value);
  commitField(mark);
}

#endif // _OTLP_H
