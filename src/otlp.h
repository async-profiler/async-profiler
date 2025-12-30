/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _OTLP_H
#define _OTLP_H

#include "engine.h"
#include "frameName.h"
#include "index.h"
#include "protobuf.h"
#include "writer.h"
#include <vector>

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

struct SampleInfo {
    u64 samples;
    u64 counter;
    size_t thread_name_index;
};

class Recorder {
  private:
    ProtoBuffer _otlp_buffer;
    FrameName& _fn;
    Index _thread_names;
    Index _functions;
    Index _strings;
    std::vector<SampleInfo> _samples_info;
    const u64 _start_nanos;
    const u64 _duration_nanos;
    const size_t _engine_type_strindex;
    const size_t _count_strindex;
    const size_t _engine_unit_strindex;

    // Record a profile with a specified sample type
    void recordOtlpProfile(size_t type_strindex, size_t unit_strindex, bool count);
    void recordSampleType(size_t type_strindex, size_t unit_strindex);
    void recordStacks(const std::vector<CallTraceSample*>& call_trace_samples);
    void recordProfilesDictionary(const std::vector<CallTraceSample*>& call_trace_samples);

  public:
    Recorder(Engine* engine, FrameName& fn, u64 start_nanos, u64 duration_nanos) :
        _otlp_buffer(ProtoBuffer(Otlp::OTLP_BUFFER_INITIAL_SIZE)),
        _fn(fn),
        _thread_names(),
        _functions(),
        _strings(),
        _samples_info(),
        _engine_type_strindex(_strings.indexOf(engine->type())),
        _engine_unit_strindex(_strings.indexOf(engine->units())),
        _count_strindex(_strings.indexOf("count")),
        _start_nanos(start_nanos),
        _duration_nanos(duration_nanos) {}

    void record(const std::vector<CallTraceSample*>& call_trace_samples);
    void write(Writer& out) {
        out.write((const char*) _otlp_buffer.data(), _otlp_buffer.offset());
    }
};

}

#endif // _OTLP_H
