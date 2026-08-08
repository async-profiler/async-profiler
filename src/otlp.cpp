/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include "callTraceStorage.h"
#include "otlp.h"
#include "vmEntry.h"

namespace Otlp {

class ResourceAttributes {
  private:
    JNIEnv* _jni;
    jclass _system_class;
    jmethodID _get_property;
    jstring _jstr;
    const char* _chars;
    char _buf[256];

    // Invalid input produces garbage, which is acceptable here
    static int hexDigit(char c) {
        return c <= '9' ? c - '0' : (c & ~0x20) - ('A' - 10);
    }

    // Copy the value until the next ',' or whitespace, decoding %XX on the way
    const char* unescape(const char* s) {
        size_t len = 0;
        while (*s > ' ' && *s != ',' && len < sizeof(_buf) - 1) {
            char c = *s++;
            if (c == '%' && s[0] && s[1]) {
                c = (char)(hexDigit(s[0]) << 4 | hexDigit(s[1]));
                s += 2;
            }
            _buf[len++] = c;
        }
        _buf[len] = 0;
        return _buf;
    }

    const char* getSystemProperty(const char* key) {
        if (_get_property == nullptr) return nullptr;
        releaseString();

        jstring jkey = _jni->NewStringUTF(key);
        _jstr = jkey == nullptr ? nullptr : (jstring)_jni->CallStaticObjectMethod(_system_class, _get_property, jkey);
        _chars = _jstr == nullptr ? nullptr : _jni->GetStringUTFChars(_jstr, nullptr);
        _jni->ExceptionClear();
        return _chars;
    }

    void releaseString() {
        if (_chars != nullptr) {
            _jni->ReleaseStringUTFChars(_jstr, _chars);
        }
    }

    // Extract attribute from a comma-separated list of key=value pairs (W3C Baggage format)
    const char* getAttribute(const char* list, const char* attr) {
        for (const char* s = strstr(list, attr); s != nullptr; s = strstr(s + 1, attr)) {
            if (s > list && s[-1] != ',' && s[-1] > ' ') {
                continue;  // not at the start of a list entry
            }

            const char* p = s + strlen(attr);
            while (*p == ' ' || *p == '\t') p++;
            if (*p++ != '=') continue;
            while (*p == ' ' || *p == '\t') p++;

            return unescape(p);
        }
        return nullptr;
    }

  public:
    ResourceAttributes() : _jni(VM::jni()), _system_class(nullptr), _get_property(nullptr), _chars(nullptr) {
        if (_jni != nullptr) {
            _jni->PushLocalFrame(16);
            if ((_system_class = _jni->FindClass("java/lang/System")) == nullptr ||
                (_get_property = _jni->GetStaticMethodID(_system_class, "getProperty", "(Ljava/lang/String;)Ljava/lang/String;")) == nullptr) {
                _jni->ExceptionClear();
            }
        }
    }

    ~ResourceAttributes() {
        if (_jni != nullptr) {
            releaseString();
            _jni->PopLocalFrame(nullptr);
        }
    }

    const char* getServiceName() {
        const char* s = getSystemProperty("otel.service.name");
        if (s != nullptr && s[0]) {
            return s;
        }

        s = getenv("OTEL_SERVICE_NAME");
        if (s != nullptr && s[0]) {
            return s;
        }

        const char* attrs = getSystemProperty("otel.resource.attributes");
        if (attrs != nullptr && (s = getAttribute(attrs, OTLP_SERVICE_NAME)) != nullptr && s[0]) {
            return s;
        }

        attrs = getenv("OTEL_RESOURCE_ATTRIBUTES");
        if (attrs != nullptr && (s = getAttribute(attrs, OTLP_SERVICE_NAME)) != nullptr && s[0]) {
            return s;
        }
        return nullptr;
    }
};

void Recorder::recordProfilesDictionary(const std::vector<CallTraceSample*>& call_trace_samples) {
    protobuf_mark_t dictionary_mark = _otlp_buffer.startMessage(ProfilesData::dictionary);

    recordStacks(call_trace_samples);

    // Write mapping_table. Not currently used, but required by some parsers
    protobuf_mark_t mapping_mark = _otlp_buffer.startMessage(ProfilesDictionary::mapping_table, 1);
    _otlp_buffer.commitMessage(mapping_mark);

    // Links are not used, but link_table[0] must be present.
    // Zero-filled 16/8 byte IDs are preferred over empty ones for compatibility
    const unsigned char zero_ids[16] = {0};
    protobuf_mark_t link_mark = _otlp_buffer.startMessage(ProfilesDictionary::link_table, 1);
    _otlp_buffer.field(Link::trace_id, zero_ids, 16);
    _otlp_buffer.field(Link::span_id, zero_ids, 8);
    _otlp_buffer.commitMessage(link_mark);

    // Write function_table
    _functions.forEachOrdered([&] (size_t idx, const std::string& function_name) {
        protobuf_mark_t function_mark = _otlp_buffer.startMessage(ProfilesDictionary::function_table, 1);
        _otlp_buffer.field(Function::name_strindex, _strings.indexOf(function_name));
        _otlp_buffer.commitMessage(function_mark);
    });

    // Write location_table; location_table[0] must be the zero value (Location{})
    for (size_t function_idx = 0; function_idx < _functions.size(); ++function_idx) {
        protobuf_mark_t location_mark = _otlp_buffer.startMessage(ProfilesDictionary::location_table, 1);
        if (function_idx != 0) {
            // TODO: set to the proper mapping when new mappings are added.
            // For now we keep a dummy default mapping_index for all locations because some parsers
            // would fail otherwise
            _otlp_buffer.field(Location::mapping_index, (u64)0);
            protobuf_mark_t line_mark = _otlp_buffer.startMessage(Location::lines, 1);
            _otlp_buffer.field(Line::function_index, function_idx);
            _otlp_buffer.commitMessage(line_mark);
        }
        _otlp_buffer.commitMessage(location_mark);
    }

    // Write attribute_table (only threads for now);
    // attribute_table[0] must be the zero value (KeyValueAndUnit{})
    size_t thread_name_key_strindex = _strings.indexOf(OTLP_THREAD_NAME);
    _thread_names.forEachOrdered([&] (size_t idx, const std::string& s) {
        protobuf_mark_t attr_mark = _otlp_buffer.startMessage(ProfilesDictionary::attribute_table);
        if (idx != 0) {
            _otlp_buffer.field(KeyValueAndUnit::key_strindex, thread_name_key_strindex);
            protobuf_mark_t value_mark = _otlp_buffer.startMessage(KeyValueAndUnit::value);
            _otlp_buffer.field(AnyValue::string_value, s.data(), s.length());
            _otlp_buffer.commitMessage(value_mark);
        }
        _otlp_buffer.commitMessage(attr_mark);
    });

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
        if (trace == nullptr || _fn.excludeTrace(trace) || cts->samples == 0) continue;

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

void Recorder::recordValueType(protobuf_index_t field_index, size_t type_strindex, size_t unit_strindex) {
    protobuf_mark_t value_type_mark = _otlp_buffer.startMessage(field_index, 1);
    _otlp_buffer.field(ValueType::type_strindex, type_strindex);
    _otlp_buffer.field(ValueType::unit_strindex, unit_strindex);
    _otlp_buffer.commitMessage(value_type_mark);
}

void Recorder::recordOtlpProfile(size_t type_strindex, size_t unit_strindex, bool samples) {
    protobuf_mark_t profile_mark = _otlp_buffer.startMessage(ScopeProfiles::profiles);

    _otlp_buffer.fieldFixed64(Profile::time_unix_nano, _start_nanos);
    _otlp_buffer.field(Profile::duration_nano, _duration_nanos);

    recordValueType(Profile::sample_type, type_strindex, unit_strindex);
    recordValueType(Profile::period_type, _engine_type_strindex, _engine_unit_strindex);
    _otlp_buffer.field(Profile::period, _period > 0 ? _period : 1);

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

void Recorder::recordResource() {
    ResourceAttributes attributes;
    const char* service_name = attributes.getServiceName();
    if (service_name == nullptr) {
        return;
    }

    protobuf_mark_t resource_mark = _otlp_buffer.startMessage(ResourceProfiles::resource);
    protobuf_mark_t attribute_mark = _otlp_buffer.startMessage(Resource::attributes);
    _otlp_buffer.field(KeyValue::key, OTLP_SERVICE_NAME);
    protobuf_mark_t value_mark = _otlp_buffer.startMessage(KeyValue::value);
    _otlp_buffer.field(AnyValue::string_value, service_name);
    _otlp_buffer.commitMessage(value_mark);
    _otlp_buffer.commitMessage(attribute_mark);
    _otlp_buffer.commitMessage(resource_mark);
}

void Recorder::record(const std::vector<CallTraceSample*>& call_trace_samples, bool samples) {
    recordProfilesDictionary(call_trace_samples);

    protobuf_mark_t resource_profiles_mark = _otlp_buffer.startMessage(ProfilesData::resource_profiles);
    recordResource();

    protobuf_mark_t scope_profiles_mark = _otlp_buffer.startMessage(ResourceProfiles::scope_profiles);
    size_t unit_strindex = samples ? _count_strindex : _engine_unit_strindex;
    recordOtlpProfile(_engine_type_strindex, unit_strindex, samples);
    _otlp_buffer.commitMessage(scope_profiles_mark);

    _otlp_buffer.commitMessage(resource_profiles_mark);
}

}
