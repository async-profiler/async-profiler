/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

class Otlp {

    public final class ProfilesDictionary {
        public static final int MAPPING_TABLE = 1;
        public static final int LOCATION_TABLE = 2;
        public static final int FUNCTION_TABLE = 3;
        public static final int STRING_TABLE = 5;
    }

    public final class ProfilesData {
        public static final int RESOURCE_PROFILES = 1;
        public static final int DICTIONARY = 2;
    }

    public final class ResourceProfiles {
        public static final int SCOPE_PROFILES = 2;
    }

    public final class ScopeProfiles {
        public static final int PROFILES = 2;
    }

    public final class Profile {
        public static final int SAMPLE_TYPE = 1;
        public static final int SAMPLE = 2;
        public static final int LOCATION_INDICES = 3;
        public static final int PERIOD_TYPE = 6;
        public static final int PERIOD = 7;
    }

    public final class ValueType {
        public static final int TYPE_STRINDEX = 1;
        public static final int UNIT_STRINDEX = 2;
        public static final int AGGREGATION_TEMPORALITY = 3;
    }

    public final class Sample {
        public static final int LOCATIONS_START_INDEX = 1;
        public static final int LOCATIONS_LENGTH = 2;
        public static final int VALUE = 3;
    }

    public final class Location {
        public static final int MAPPING_INDEX = 1;
        public static final int LINE = 3;
    }

    public final class Function {
        public static final int NAME_STRINDEX = 1;
        public static final int FILENAME_STRINDEX = 3;
    }

    public final class Line {
        public static final int FUNCTION_INDEX = 1;
    }

    public final class AggregationTemporality {
        public static final int UNSPECIFIED = 0;
        public static final int DELTA = 1;
        public static final int CUMULATIVE = 2;
    }

}