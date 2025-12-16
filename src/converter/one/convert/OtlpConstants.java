/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

final class OtlpConstants {

    static final String OTLP_THREAD_NAME = "thread.name";

    static final int
            PROFILES_DICTIONARY_mapping_table = 1,
            PROFILES_DICTIONARY_location_table = 2,
            PROFILES_DICTIONARY_function_table = 3,
            PROFILES_DICTIONARY_string_table = 5,
            PROFILES_DICTIONARY_attribute_table = 6,
            PROFILES_DICTIONARY_stack_table = 7;

    static final int
            PROFILES_DATA_resource_profiles = 1,
            PROFILES_DATA_dictionary = 2;

    static final int RESOURCE_PROFILES_scope_profiles = 2;

    static final int SCOPE_PROFILES_profiles = 2;

    static final int
            PROFILE_sample_type = 1,
            PROFILE_samples = 2,
            PROFILE_time_unix_nano = 3,
            PROFILE_duration_nanos = 4;

    static final int
            VALUE_TYPE_type_strindex = 1,
            VALUE_TYPE_unit_strindex = 2,
            VALUE_TYPE_aggregation_temporality = 3;

    static final int
            SAMPLE_stack_index = 1,
            SAMPLE_values = 2,
            SAMPLE_attribute_indices = 3,
            SAMPLE_timestamps_unix_nano = 5;

    static final int
            STACK_location_indices = 1;

    static final int
            LOCATION_mapping_index = 1,
            LOCATION_line = 3;

    static final int
            LINE_function_index = 1,
            LINE_lines = 2;

    static final int FUNCTION_name_strindex = 1;

    static final int AGGREGATION_TEMPORARALITY_cumulative = 2;

    static final int
            KEY_VALUE_AND_UNIT_key_strindex = 1,
            KEY_VALUE_AND_UNIT_value = 2;

    static final int ANY_VALUE_string_value = 1;
}
