/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OTLP_H
#define _OTLP_H

#include "protobuf.h"

namespace Otlp {

namespace ProfilesDictionary {
    const protobuf_index_t mapping_table = 1;
    const protobuf_index_t location_table = 2;
    const protobuf_index_t function_table = 3;
    const protobuf_index_t string_table = 5;
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
    const protobuf_index_t sample = 2;
    const protobuf_index_t location_indices = 3;
    const protobuf_index_t period_type = 6;
    const protobuf_index_t period = 7;
}

namespace ValueType {
    const protobuf_index_t type_strindex = 1;
    const protobuf_index_t unit_strindex = 2;
    const protobuf_index_t aggregation_temporality = 3;
}

namespace Sample {
    const protobuf_index_t locations_start_index = 1;
    const protobuf_index_t locations_length = 2;
    const protobuf_index_t value = 3;
}

namespace Location {
    const protobuf_index_t mapping_index = 1;
    const protobuf_index_t line = 3;
}

namespace Function {
    const protobuf_index_t name_strindex = 1;
    const protobuf_index_t filename_strindex = 3;
}

namespace Line {
    const protobuf_index_t function_index = 1;
}

namespace AggregationTemporality {
    const u64 unspecified = 0;
    const u64 delta = 1;
    const u64 cumulative = 2;
}

}

#endif // _OTLP_H
