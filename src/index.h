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
  
  public:
    Index() = default;

    Index(const Index&) = delete;
    Index(Index&&) = delete;
    Index& operator=(const Index&) = delete;
    Index& operator=(Index&&) = delete;

    size_t indexOf(const std::string& value) {
        return _idx_map.insert({value, _idx_map.size()}).first->second;
    }

    size_t size() const {
        return _idx_map.size();
    }

    void forEachOrdered(std::function<void(const std::string&)> consumer) const {
        std::vector<const std::string*> arr(_idx_map.size());
        for (const auto& it : _idx_map) {
            arr[it.second] = &it.first;
        }
        for (size_t idx = 0; idx < size(); ++idx) {
            consumer(*arr[idx]);
        }
    }
};

#endif // _INDEX_H
