/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OTLP_H
#define _OTLP_H

#include "protobuf.h"

namespace Otlp {

const u32 OTLP_BUFFER_INITIAL_SIZE = 5120;
// https://opentelemetry.io/docs/specs/semconv/registry/attributes/thread/#thread-name
const char* const OTLP_THREAD_NAME = "thread.name";

namespace ProfilesDictionary {
    const protobuf_index_t mapping_table = 1;
    const protobuf_index_t location_table = 2;
    const protobuf_index_t function_table = 3;
    const protobuf_index_t string_table = 5;
    const protobuf_index_t attribute_table = 6;
    const protobuf_index_t stack_table = 7;
}

namespace ProfilesData {
    const protobuf_index_t resource_profiles = 1;
    const protobuf_index_t dictionary = 2;
}

namespace ResourceProfiles {
    const protobuf_index_t scope_profiles = 2;
}

namespace ScopeProfiles {
    const protobuf_index_t profiles = 2;
}

namespace Profile {
    const protobuf_index_t sample_type = 1;
    const protobuf_index_t samples = 2;
    const protobuf_index_t time_unix_nano = 3;
    const protobuf_index_t duration_nano = 4;
}

namespace ValueType {
    const protobuf_index_t type_strindex = 1;
    const protobuf_index_t unit_strindex = 2;
    const protobuf_index_t aggregation_temporality = 3;
}

namespace Sample {
    const protobuf_index_t stack_index = 1;
    const protobuf_index_t values = 2;
    const protobuf_index_t attribute_indices = 3;
}

namespace Stack {
    const protobuf_index_t location_indices = 1;
}

namespace Location {
    const protobuf_index_t mapping_index = 1;
    const protobuf_index_t lines = 3;
}

namespace Function {
    const protobuf_index_t name_strindex = 1;
    const protobuf_index_t filename_strindex = 3;
}

namespace Line {
    const protobuf_index_t function_index = 1;
}

namespace KeyValueAndUnit {
    const protobuf_index_t key_strindex = 1;
    const protobuf_index_t value = 2;
}

namespace AnyValue {
    const protobuf_index_t string_value = 1;
}

}

#endif // _OTLP_H
