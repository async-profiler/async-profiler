/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _INDEX_H
#define _INDEX_H

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include "arch.h"

// Keeps track of values seen and their index of occurrence
class Index {
  private:
    std::unordered_map<std::string, size_t> _idx_map;
    size_t _start_index;
  
  public:
    Index(size_t start_index = 0) : _start_index(start_index) {
        // The first index should contain the empty string
        indexOf("");
    }

    Index(const Index&) = delete;
    Index(Index&&) = delete;
    Index& operator=(const Index&) = delete;
    Index& operator=(Index&&) = delete;

    size_t indexOf(const char* value) {
        return indexOf(std::string(value));
    }

    size_t indexOf(const char* value, size_t len) {
        return indexOf(std::string(value, len));
    }

    size_t indexOf(const std::string& value) {
        return _idx_map.insert({value, _start_index + _idx_map.size()}).first->second;
    }

    size_t indexOf(std::string&& value) {
        return _idx_map.insert({std::move(value), _start_index + _idx_map.size()}).first->second;
    }

    size_t size() const {
        return _idx_map.size();
    }

    void forEachOrdered(const std::function<void(size_t idx, const std::string&)>& consumer) const {
        std::vector<const std::string*> arr(_idx_map.size());
        for (const auto& it : _idx_map) {
            arr[it.second - _start_index] = &it.first;
        }
        for (size_t idx = 0; idx < size(); ++idx) {
            consumer(idx + _start_index, *arr[idx]);
        }
    }
};

#endif // _INDEX_H
