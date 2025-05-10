/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PROTOBUF_H
#define _PROTOBUF_H

#include "arch.h"
#include "os.h"
#include <string.h>
#include <type_traits>

typedef const u8 protobuf_t;
static protobuf_t VARINT = 0;
static protobuf_t I64 = 1;
static protobuf_t LEN = 2;
static protobuf_t I32 = 5;

typedef u8 protobuf_index_t;
typedef size_t protobuf_field_mark_t;

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
  int skip(size_t delta) {
    size_t offset = _offset;
    _offset = offset + delta;
    return offset;
  }

  void put8(char v) {
    put8(_offset++, v);
  }

  void put8(size_t offset, char v) {
    _data[offset] = v;
  }

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

  void tag(protobuf_index_t index, protobuf_t type);

public:
  ProtobufBuffer(char *data) : LittleEndianBuffer(data) {}

  const char *data() const { return _data; }

  size_t offset() const { return _offset; }

  // VARINT
  void field(protobuf_index_t index, bool b);
  void field(protobuf_index_t index, u32 n);
  void field(protobuf_index_t index, u64 n);
  // I32
  void field(protobuf_index_t index, int n);
  void field(protobuf_index_t index, float n);
  // I64
  void field(protobuf_index_t index, long n);
  void field(protobuf_index_t index, double n);
  // LEN
  void field(protobuf_index_t index, const char *s);
  void field(protobuf_index_t index, const char *s, size_t len);
  void field(protobuf_index_t index, const ProtobufBuffer buffer, size_t len);

  size_t startMessage(protobuf_index_t index);
  void commitMessage(protobuf_field_mark_t mark);

  template <typename K, typename V>
  void mapEntry(int mapIndex, K key, V value);
};

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, void>::type
ProtobufBuffer::putVarInt(T n) {
  _offset = putVarInt(_offset, n);
}

template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, size_t>::type
ProtobufBuffer::putVarInt(size_t offset, T n) {
  while ((n >> 7) != 0) {
    _data[offset++] = (char)(0b10000000 | (n & 0b01111111));
    n >>= 7;
  }
  _data[offset++] = (char)n;
  return offset;
}

template <typename K, typename V>
void ProtobufBuffer::mapEntry(int mapIndex, K key, V value) {
  protobuf_field_mark_t mark = startMessage(mapIndex);
  field(1, key);
  field(2, value);
  commitMessage(mark);
}

#endif // _PROTOBUF_H
