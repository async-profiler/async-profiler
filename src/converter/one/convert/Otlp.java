/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

final class Otlp {

    enum ProfilesDictionary {
        MAPPING_TABLE(1),
        LOCATION_TABLE(2),
        FUNCTION_TABLE(3),
        STRING_TABLE(5);

        public final int index;

        ProfilesDictionary(int index) {
            this.index = index;
        }
    }

    enum ProfilesData {
        RESOURCE_PROFILES(1),
        DICTIONARY(2);

        public final int index;

        ProfilesData(int index) {
            this.index = index;
        }
    }

    enum ResourceProfiles {
        SCOPE_PROFILES(2);

        public final int index;

        ResourceProfiles(int index) {
            this.index = index;
        }
    }

    enum ScopeProfiles {
        PROFILES(2);

        public final int index;

        ScopeProfiles(int index) {
            this.index = index;
        }
    }

    enum Profile {
        SAMPLE_TYPE(1),
        SAMPLE(2),
        LOCATION_INDICES(3),
        TIME_NANOS(4),
        DURATION_NANOS(5),
        ORIGINAL_PAYLOAD(13);

        public final int index;

        Profile(int index) {
            this.index = index;
        }
    }

    enum ValueType {
        TYPE_STRINDEX(1),
        UNIT_STRINDEX(2),
        AGGREGATION_TEMPORALITY(3);

        public final int index;

        ValueType(int index) {
            this.index = index;
        }
    }

    enum Sample {
        LOCATIONS_START_INDEX(1),
        LOCATIONS_LENGTH(2),
        VALUE(3);

        public final int index;

        Sample(int index) {
            this.index = index;
        }
    }

    enum Location {
        MAPPING_INDEX(1),
        LINE(3);

        public final int index;

        Location(int index) {
            this.index = index;
        }
    }

    enum Function {
        NAME_STRINDEX(1);

        public final int index;

        Function(int index) {
            this.index = index;
        }
    }

    enum Line {
        FUNCTION_INDEX(1);

        public final int index;

        Line(int index) {
            this.index = index;
        }
    }

    enum AggregationTemporality {
        CUMULATIVE(2);

        public final int value;

        AggregationTemporality(int value) {
            this.value = value;
        }
    }
}
