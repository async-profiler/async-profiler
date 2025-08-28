/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INDEX_H
#define _INDEX_H

#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "arch.h"

// Keeps track of values seen and their index of occurrence
template<typename T>
class GenericIndex {
    static_assert(std::is_default_constructible<T>::value, "Template parameter T must be default constructible");

  private:
    std::unordered_map<T, size_t> _idx_map;
    size_t _start_index;
  
  public:
    GenericIndex(size_t start_index = 0) : _start_index(start_index) {
        // The first index should contain the empty string
        indexOf(T());
    }

    GenericIndex(const GenericIndex&) = delete;
    GenericIndex(GenericIndex&&) = delete;
    GenericIndex& operator=(const GenericIndex&) = delete;
    GenericIndex& operator=(GenericIndex&&) = delete;

    size_t indexOf(const T& value) {
        return _idx_map.insert({value, _idx_map.size()}).first->second;
    }

    size_t indexOf(T&& value) {
        return _idx_map.insert({std::move(value), _idx_map.size()}).first->second;
    }

    size_t size() const {
        return _idx_map.size();
    }

    void forEachOrdered(const std::function<void(size_t idx, const T&)>& consumer) const {
        std::vector<const T*> arr(_idx_map.size());
        for (const auto& it : _idx_map) {
            arr[it.second - _start_index] = &it.first;
        }
        for (size_t idx = 0; idx < size(); ++idx) {
            consumer(idx + _start_index, *arr[idx]);
        }
    }
};

class Index : public GenericIndex<std::string> {
  public:
    using GenericIndex<std::string>::indexOf;

    Index(size_t start_index = 0) : GenericIndex(start_index) {}

    size_t indexOf(const char* value) {
        return indexOf(std::string(value));
    }

    size_t indexOf(const char* value, size_t len) {
        return indexOf(std::string(value, len));
    }
};

#endif // _INDEX_H
