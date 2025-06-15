/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OTLP_H
#define _OTLP_H

#include <cstdint>
#include "protobuf.h"

#define NO_COPY_MOVE(TypeName)                   \
  TypeName(const TypeName&) = delete;            \
  TypeName(TypeName&&) = delete;                 \
  TypeName& operator=(const TypeName&) = delete; \
  TypeName& operator=(TypeName&&) = delete;      \

namespace Otlp {

const u32 OTLP_BUFFER_INITIAL_SIZE = 5120;

namespace ProfilesDictionary {
    const protobuf_index_t MAPPING_TABLE = 1;
    const protobuf_index_t LOCATION_TABLE = 2;
    const protobuf_index_t FUNCTION_TABLE = 3;
    const protobuf_index_t STRING_TABLE = 5;
}

namespace ProfilesData {
    const protobuf_index_t RESOURCE_PROFILES = 1;
    const protobuf_index_t DICTIONARY = 2;
}

namespace ResourceProfiles {
    const protobuf_index_t SCOPE_PROFILES = 2;
}

namespace ScopeProfiles {
    const protobuf_index_t PROFILES = 2;
}

namespace Profile {
    const protobuf_index_t SAMPLE_TYPE = 1;
    const protobuf_index_t SAMPLE = 2;
    const protobuf_index_t LOCATION_INDICES = 3;
    const protobuf_index_t PERIOD_TYPE = 6;
    const protobuf_index_t PERIOD = 7;
}

namespace ValueType {
    const protobuf_index_t TYPE_STRINDEX = 1;
    const protobuf_index_t UNIT_STRINDEX = 2;
    const protobuf_index_t AGGREGATION_TEMPORALITY = 3;
}

namespace Sample {
    const protobuf_index_t LOCATIONS_START_INDEX = 1;
    const protobuf_index_t LOCATIONS_LENGTH = 2;
    const protobuf_index_t VALUE = 3;
}

class Line {
  public:
    static const protobuf_index_t FUNCTION_INDEX = 1;
    static const protobuf_index_t LINE = 2;

    const size_t function_index;
    const u64 line;

    Line(size_t function_index, u64 line) : function_index(function_index), line(line) {}

    bool operator==(const Line& other) const {
        return function_index == other.function_index && line == other.line;
    }
};

class Location {
  public:
    static const protobuf_index_t MAPPING_INDEX = 1;
    static const protobuf_index_t ADDRESS = 2;
    static const protobuf_index_t LINE = 3;

    const size_t mapping_index;
    const u64 address;
    const Line line;

    Location(size_t mapping_index, u64 address, Line line) :
        mapping_index(mapping_index), address(address), line(line) {}

    bool operator==(const Location& other) const {
        return mapping_index == other.mapping_index
            && address == other.address
            && line == other.line;
    }
};

class Function {
  public:
    static const protobuf_index_t NAME_STRINDEX = 1;
    static const protobuf_index_t FILENAME_STRINDEX = 3;

    const size_t name_strindex;
    const size_t system_name_strindex;
    const size_t file_name_strindex;

    Function(size_t name_strindex, size_t system_name_strindex, size_t file_name_strindex) :
        name_strindex(name_strindex), system_name_strindex(system_name_strindex), file_name_strindex(file_name_strindex) {}

    bool operator==(const Function& other) const {
        return name_strindex == other.name_strindex
            && system_name_strindex == other.system_name_strindex
            && file_name_strindex == other.file_name_strindex;
    }
};

namespace AggregationTemporality {
    const u64 unspecified = 0;
    const u64 delta = 1;
    const u64 cumulative = 2;
}

class Mapping {
  public:
    static const protobuf_index_t MEMORY_START = 1;
    static const protobuf_index_t MEMORY_LIMIT = 2;
    static const protobuf_index_t FILENAME_STRINDEX = 4;

    const uintptr_t memory_start;
    const uintptr_t memory_limit;
    const size_t file_name_strindex;

    Mapping(uintptr_t memory_start, uintptr_t memory_limit, size_t file_name_strindex) :
        memory_start(memory_start), memory_limit(memory_limit), file_name_strindex(file_name_strindex) {}

    bool operator==(const Mapping& other) const {
        return memory_start == other.memory_start
            && memory_limit == other.memory_limit
            && file_name_strindex == other.file_name_strindex;
    }
};

}

namespace std {
    template <>
    struct hash<Otlp::Mapping> {
        size_t operator()(const Otlp::Mapping& m) const {
            size_t h1 = std::hash<size_t>()(m.file_name_strindex);
            size_t h2 = std::hash<uintptr_t>()(m.memory_start);
            size_t h3 = std::hash<uintptr_t>()(m.memory_limit);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    template <>
    struct hash<Otlp::Function> {
        size_t operator()(const Otlp::Function& f) const {
            size_t h1 = std::hash<size_t>()(f.name_strindex);
            size_t h2 = std::hash<u64>()(f.file_name_strindex);
            return h1 ^ (h2 << 1);
        }
    };

    template <>
    struct hash<Otlp::Location> {
        size_t operator()(const Otlp::Location& l) const {
            size_t h1 = std::hash<size_t>()(l.mapping_index);
            size_t h2 = std::hash<u64>()(l.address);
            size_t h3 = std::hash<size_t>()(l.line.function_index);
            size_t h4 = std::hash<u64>()(l.line.line);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
        }
    };
}

#endif // _OTLP_H
