/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

final class OtlpConstants {

    // https://github.com/open-telemetry/semantic-conventions/blob/main/docs/general/profiles.md
    static final String FRAME_TYPE_ATTRIBUTE_KEY = "profile.frame.type";

    static final int
            PROFILES_DICTIONARY_mapping_table = 1,
            PROFILES_DICTIONARY_location_table = 2,
            PROFILES_DICTIONARY_function_table = 3,
            PROFILES_DICTIONARY_string_table = 5,
            PROFILES_DICTIONARY_attribute_table = 6;

    static final int
            PROFILES_DATA_resource_profiles = 1,
            PROFILES_DATA_dictionary = 2;

    static final int RESOURCE_PROFILES_scope_profiles = 2;

    static final int SCOPE_PROFILES_profiles = 2;

    static final int
            PROFILE_sample_type = 1,
            PROFILE_sample = 2,
            PROFILE_location_indices = 3,
            PROFILE_time_nanos = 4,
            PROFILE_duration_nanos = 5,
            PROFILE_original_payload = 13;

    static final int
            VALUE_TYPE_type_strindex = 1,
            VALUE_TYPE_unit_strindex = 2,
            VALUE_TYPE_aggregation_temporality = 3;

    static final int
            SAMPLE_locations_start_index = 1,
            SAMPLE_locations_length = 2,
            SAMPLE_value = 3;

    static final int
            LOCATION_mapping_index = 1,
            LOCATION_line = 3,
            LOCATION_attribute_indices = 5;

    static final int
            LINE_function_index = 1,
            LINE_line = 2;

    static final int FUNCTION_name_strindex = 1;

    static final int AGGREGATION_TEMPORARALITY_cumulative = 2;

    static final int
            KEY_VALUE_key = 1,
            KEY_VALUE_value = 2;
}
