/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arch.h"
#include "buffer.h"
#include <type_traits>

typedef const u8 protobuf_t;

class ProtobufBuffer : private Buffer {
  private:
    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, void>::type putVarInt(T n);

    template <typename T>
    typename std::enable_if<std::is_unsigned<T>::value, std::size_t>::type putVarInt(std::size_t offset, T n);

    void tag(int index, protobuf_t type);

  public:
    ProtobufBuffer(char* data) : Buffer() {}

    void field(int index, int n);
    void field(int index, u32 n);
    void field(int index, long n);
    void field(int index, u64 n);
    void field(int index, float n);
    void field(int index, double n);
    void field(int index, const char* s);
    void field(int index, const char* s, size_t len);
    void field(int index, const Buffer buffer, size_t len);

    std::size_t startField(int index);
    void commitField(std::size_t mark);
};
