/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "callTraceStorage.h"
#include "otlp.h"

namespace Otlp {

void Recorder::recordProfilesDictionary(const std::vector<CallTraceSample*>& call_trace_samples) {
    protobuf_mark_t dictionary_mark = _otlp_buffer.startMessage(ProfilesData::dictionary);

    recordStacks(call_trace_samples);

    // Write mapping_table. Not currently used, but required by some parsers
    protobuf_mark_t mapping_mark = _otlp_buffer.startMessage(ProfilesDictionary::mapping_table, 1);
    _otlp_buffer.commitMessage(mapping_mark);

    // Write function_table
    _functions.forEachOrdered([&] (size_t idx, const std::string& function_name) {
        protobuf_mark_t function_mark = _otlp_buffer.startMessage(ProfilesDictionary::function_table, 1);
        _otlp_buffer.field(Function::name_strindex, _strings.indexOf(function_name));
        _otlp_buffer.commitMessage(function_mark);
    });

    // Write location_table
    for (size_t function_idx = 0; function_idx < _functions.size(); ++function_idx) {
        protobuf_mark_t location_mark = _otlp_buffer.startMessage(ProfilesDictionary::location_table, 1);
        // TODO: set to the proper mapping when new mappings are added.
        // For now we keep a dummy default mapping_index for all locations because some parsers
        // would fail otherwise
        _otlp_buffer.field(Location::mapping_index, (u64)0);
        protobuf_mark_t line_mark = _otlp_buffer.startMessage(Location::lines, 1);
        _otlp_buffer.field(Line::function_index, function_idx);
        _otlp_buffer.commitMessage(line_mark);
        _otlp_buffer.commitMessage(location_mark);
    }

    // Write attribute_table (only threads for now)
    if (!_thread_names.empty()) {
        size_t thread_name_key_strindex = _strings.indexOf(OTLP_THREAD_NAME);
        _thread_names.forEachOrdered([&] (size_t idx, const std::string& s) {
            protobuf_mark_t attr_mark = _otlp_buffer.startMessage(ProfilesDictionary::attribute_table);
            _otlp_buffer.field(KeyValueAndUnit::key_strindex, thread_name_key_strindex);
            protobuf_mark_t value_mark = _otlp_buffer.startMessage(KeyValueAndUnit::value);
            _otlp_buffer.field(AnyValue::string_value, s.data(), s.length());
            _otlp_buffer.commitMessage(value_mark);
            _otlp_buffer.commitMessage(attr_mark);
        });
    }

    // Write string_table
    _strings.forEachOrdered([&] (size_t idx, const std::string& s) {
        _otlp_buffer.field(ProfilesDictionary::string_table, s.data(), s.length());
    });

    _otlp_buffer.commitMessage(dictionary_mark);
}

void Recorder::recordStacks(const std::vector<CallTraceSample*>& call_trace_samples) {
    {
        // stack_table[0] must always be zero value (Stack{}) and present.
        protobuf_mark_t stack_mark = _otlp_buffer.startMessage(ProfilesDictionary::stack_table);
        _otlp_buffer.commitMessage(stack_mark);
    }

    for (const auto& cts : call_trace_samples) {
        CallTrace* trace = cts->acquireTrace();
        if (trace == NULL || _fn.excludeTrace(trace) || cts->samples == 0) continue;

        protobuf_mark_t stack_mark = _otlp_buffer.startMessage(ProfilesDictionary::stack_table);
        protobuf_mark_t location_indices_mark = _otlp_buffer.startMessage(Stack::location_indices);
        size_t thread_name_index_value = 0;
        for (int j = 0; j < trace->num_frames; j++) {
            if (trace->frames[j].bci == BCI_THREAD_ID) {
                thread_name_index_value = _thread_names.indexOf(_fn.name(trace->frames[j]));
                continue;
            }

            size_t location_idx = _functions.indexOf(_fn.name(trace->frames[j]));
            _otlp_buffer.putVarInt(location_idx);
        }
        _otlp_buffer.commitMessage(location_indices_mark);
        _otlp_buffer.commitMessage(stack_mark);

        _samples_info.push_back(SampleInfo{cts->samples, cts->counter, thread_name_index_value});
    }
}

void Recorder::recordSampleType(size_t type_strindex, size_t unit_strindex) {
    protobuf_mark_t sample_type_mark = _otlp_buffer.startMessage(Profile::sample_type, 1);
    _otlp_buffer.field(ValueType::type_strindex, type_strindex);
    _otlp_buffer.field(ValueType::unit_strindex, unit_strindex);
    _otlp_buffer.commitMessage(sample_type_mark);
}

void Recorder::recordOtlpProfile(size_t type_strindex, size_t unit_strindex, bool samples) {
    protobuf_mark_t profile_mark = _otlp_buffer.startMessage(ScopeProfiles::profiles);

    _otlp_buffer.fieldFixed64(Profile::time_unix_nano, _start_nanos);
    _otlp_buffer.field(Profile::duration_nano, _duration_nanos);

    recordSampleType(type_strindex, unit_strindex);

    for (size_t i = 0; i < _samples_info.size(); ++i) {
        const SampleInfo& si = _samples_info[i];
        protobuf_mark_t sample_mark = _otlp_buffer.startMessage(Profile::samples, 1);
        // stack_table[0] is the empty stack
        _otlp_buffer.field(Sample::stack_index, i + 1);
        if (si.thread_name_index != 0) {
            _otlp_buffer.field(Sample::attribute_indices, si.thread_name_index);
        }
        _otlp_buffer.field(Sample::values, samples ? si.samples : si.counter);
        _otlp_buffer.commitMessage(sample_mark);
    }

    _otlp_buffer.commitMessage(profile_mark);
}

void Recorder::record(const std::vector<CallTraceSample*>& call_trace_samples, bool samples) {
    recordProfilesDictionary(call_trace_samples);

    protobuf_mark_t resource_profiles_mark = _otlp_buffer.startMessage(ProfilesData::resource_profiles);
    protobuf_mark_t scope_profiles_mark = _otlp_buffer.startMessage(ResourceProfiles::scope_profiles);

    size_t unit_strindex = samples ? _strings.indexOf("count") : _engine_unit_strindex;
    recordOtlpProfile(_engine_type_strindex, unit_strindex, samples);

    _otlp_buffer.commitMessage(scope_profiles_mark);
    _otlp_buffer.commitMessage(resource_profiles_mark);
}

}
