/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

class Otlp {

    static final class ProfilesDictionary {
        static final int MAPPING_TABLE = 1;
        static final int LOCATION_TABLE = 2;
        static final int FUNCTION_TABLE = 3;
        static final int STRING_TABLE = 5;
    }

    static final class ProfilesData {
        static final int RESOURCE_PROFILES = 1;
        static final int DICTIONARY = 2;
    }

    static final class ResourceProfiles {
        static final int SCOPE_PROFILES = 2;
    }

    static final class ScopeProfiles {
        static final int PROFILES = 2;
    }

    static final class Profile {
        static final int SAMPLE_TYPE = 1;
        static final int SAMPLE = 2;
        static final int LOCATION_INDICES = 3;
        static final int TIME_NANOS = 4;
        static final int DURATION_NANOS = 5;
        static final int ORIGINAL_PAYLOAD = 13;
    }

    static final class ValueType {
        static final int TYPE_STRINDEX = 1;
        static final int UNIT_STRINDEX = 2;
        static final int AGGREGATION_TEMPORALITY = 3;
    }

    static final class Sample {
        static final int LOCATIONS_START_INDEX = 1;
        static final int LOCATIONS_LENGTH = 2;
        static final int VALUE = 3;
    }

    static final class Location {
        static final int MAPPING_INDEX = 1;
        static final int LINE = 3;
    }

    static final class Function {
        static final int NAME_STRINDEX = 1;
    }

    static final class Line {
        static final int FUNCTION_INDEX = 1;
    }

    static final class AggregationTemporality {
        static final int CUMULATIVE = 2;
    }
}
