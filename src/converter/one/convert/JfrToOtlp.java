/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.Event;
import one.jfr.event.EventCollector;
import one.proto.Proto;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.*;

import static one.convert.OtlpConstants.*;

/** Converts .jfr output to OpenTelemetry protocol. */
public class JfrToOtlp extends JfrConverter {
    private static final int BIG_MESSAGE_BYTE_COUNT = 5;

    private final Index<String> stringPool = new Index<>(String.class, "");
    private final Index<String> functionPool = new Index<>(String.class, "");
    private final Index<Line> linePool = new Index<>(Line.class, Line.EMPTY);
    private final Index<KeyValue> attributesPool = new Index<>(KeyValue.class, KeyValue.EMPTY);

    private final Proto otlpProto = new Proto(1024);

    public JfrToOtlp(JfrReader jfr, Arguments args) {
        super(jfr, args);

        // string_table[0] must always be the empty string
        stringPool.index("");
    }

    @Override
    public void convert() throws IOException {
        long resourceProfilesMark =
                otlpProto.startField(PROFILES_DATA_resource_profiles, BIG_MESSAGE_BYTE_COUNT);
        long scopeProfilesMark =
                otlpProto.startField(RESOURCE_PROFILES_scope_profiles, BIG_MESSAGE_BYTE_COUNT);
        super.convert();
        otlpProto.commitField(scopeProfilesMark);
        otlpProto.commitField(resourceProfilesMark);

        writeProfileDictionary();
    }

    @Override
    protected void convertChunk() {
        long profileMark = otlpProto.startField(SCOPE_PROFILES_profiles, BIG_MESSAGE_BYTE_COUNT);

        writeSampleTypes();
        writeTimingInformation();

        List<Integer> locationIndices = new ArrayList<>();
        collector.forEach(new OtlpEventToSampleVisitor(locationIndices));

        long locationIndicesMark =
                otlpProto.startField(PROFILE_location_indices, BIG_MESSAGE_BYTE_COUNT);
        locationIndices.forEach(otlpProto::writeInt);
        otlpProto.commitField(locationIndicesMark);

        otlpProto.commitField(profileMark);
    }

    private void writeSampleTypes() {
        long sampleTypeSamplesMark = otlpProto.startField(PROFILE_sample_type, 1);
        otlpProto.field(VALUE_TYPE_type_strindex, stringPool.index(getValueType()));
        otlpProto.field(VALUE_TYPE_unit_strindex, stringPool.index(getSampleUnits()));
        otlpProto.field(VALUE_TYPE_aggregation_temporality, AGGREGATION_TEMPORARALITY_cumulative);
        otlpProto.commitField(sampleTypeSamplesMark);

        long sampleTypeTotalMark = otlpProto.startField(PROFILE_sample_type, 1);
        otlpProto.field(VALUE_TYPE_type_strindex, stringPool.index(getValueType()));
        otlpProto.field(VALUE_TYPE_unit_strindex, stringPool.index(getTotalUnits()));
        otlpProto.field(VALUE_TYPE_aggregation_temporality, AGGREGATION_TEMPORARALITY_cumulative);
        otlpProto.commitField(sampleTypeTotalMark);
    }

    private void writeTimingInformation() {
        otlpProto.field(PROFILE_time_nanos, jfr.getChunkStartNanos());
        otlpProto.field(PROFILE_duration_nanos, jfr.chunkDurationNanos());
    }

    public void dump(OutputStream out) throws IOException {
        out.write(otlpProto.buffer(), 0, otlpProto.size());
    }

    private void writeProfileDictionary() {
        long profilesDictionaryMark =
                otlpProto.startField(PROFILES_DATA_dictionary, BIG_MESSAGE_BYTE_COUNT);

        // Mapping[0] must be a default mapping according to the spec
        long mappingMark = otlpProto.startField(PROFILES_DICTIONARY_mapping_table, 1);
        otlpProto.commitField(mappingMark);

        // Write function table
        for (String functionName : functionPool.keys()) {
            long functionMark = otlpProto.startField(PROFILES_DICTIONARY_function_table, 1);
            int functionNameStrindex = stringPool.index(functionName);
            otlpProto.field(FUNCTION_name_strindex, functionNameStrindex);
            otlpProto.commitField(functionMark);
        }

        // Write location table
        for (Line line : linePool.keys()) {
            long locationMark = otlpProto.startField(PROFILES_DICTIONARY_location_table, 1);
            otlpProto.field(LOCATION_mapping_index, 0);

            long lineMark = otlpProto.startField(LOCATION_line, 1);
            otlpProto.field(LINE_function_index, line.functionIdx);
            otlpProto.field(LINE_line, line.lineNumber);
            otlpProto.commitField(lineMark);

            otlpProto.commitField(locationMark);
        }

        // Write string table
        for (String s : stringPool.keys()) {
            otlpProto.field(PROFILES_DICTIONARY_string_table, s);
        }

        // Write attributes table
        for (KeyValue keyValue : attributesPool.keys()) {
            long attributeMark =
                    otlpProto.startField(
                            PROFILES_DICTIONARY_attribute_table, BIG_MESSAGE_BYTE_COUNT);
            otlpProto.field(KEY_VALUE_key, keyValue.key);

            long valueMark = otlpProto.startField(KEY_VALUE_value, BIG_MESSAGE_BYTE_COUNT);
            otlpProto.field(ANY_VALUE_string_value, keyValue.value);
            otlpProto.commitField(valueMark);

            otlpProto.commitField(attributeMark);
        }

        otlpProto.commitField(profilesDictionaryMark);
    }

    public static void convert(String input, String output, Arguments args) throws IOException {
        JfrToOtlp converter;
        try (JfrReader jfr = new JfrReader(input)) {
            converter = new JfrToOtlp(jfr, args);
            converter.convert();
        }
        try (FileOutputStream out = new FileOutputStream(output)) {
            converter.dump(out);
        }
    }

    private final class OtlpEventToSampleVisitor implements EventCollector.Visitor {
        private final List<Integer> locationIndices;
        private final double factor = counterFactor();

        // JFR constant pool stacktrace ID to Range
        private final Map<Integer, Range> idToRange = new HashMap<>();
        // Next index to be used for a location into Profile.location_indices
        private int nextLocationIdx = 0;

        public OtlpEventToSampleVisitor(List<Integer> locationIndices) {
            this.locationIndices = locationIndices;
        }

        @Override
        public void visit(Event event, long samples, long value) {
            long msFromStart = (event.time - jfr.chunkStartTicks) * 1_000 / jfr.ticksPerSec;
            long timeNanos = jfr.chunkStartNanos + msFromStart * 1_000_000;

            Range range = idToRange.computeIfAbsent(event.stackTraceId, this::computeLocationRange);

            long sampleMark = otlpProto.startField(PROFILE_sample, 1);
            otlpProto.field(SAMPLE_locations_start_index, range.start);
            otlpProto.field(SAMPLE_locations_length, range.length);
            otlpProto.field(SAMPLE_timestamps_unix_nano, timeNanos);

            KeyValue threadNameAttribute = new KeyValue("thread.name", getThreadName(event.tid));
            otlpProto.field(SAMPLE_attribute_indices, attributesPool.index(threadNameAttribute));

            long sampleValueMark = otlpProto.startField(SAMPLE_value, 1);
            otlpProto.writeLong(samples);
            otlpProto.writeLong(factor == 1.0 ? value : (long) (value * factor));
            otlpProto.commitField(sampleValueMark);

            otlpProto.commitField(sampleMark);
        }

        // Range of values in Profile.location_indices
        private Range computeLocationRange(int stackTraceId) {
            StackTrace stackTrace = jfr.stackTraces.get(stackTraceId);
            if (stackTrace == null) {
                return new Range(0, 0);
            }
            for (int i = 0; i < stackTrace.methods.length; ++i) {
                locationIndices.add(linePool.index(makeLine(stackTrace, i)));
            }
            Range range = new Range(nextLocationIdx, stackTrace.methods.length);
            nextLocationIdx += stackTrace.methods.length;
            return range;
        }

        private Line makeLine(StackTrace stackTrace, int i) {
            String methodName = getMethodName(stackTrace.methods[i], stackTrace.types[i]);
            int lineNumber = stackTrace.locations[i] >>> 16;
            int functionIdx = functionPool.index(methodName);
            return new Line(functionIdx, lineNumber);
        }
    }

    private static final class Range {
        final int start;
        final int length;

        public Range(int start, int length) {
            this.start = start;
            this.length = length;
        }
    }

    private static final class Line {
        static final Line EMPTY = new Line(0, 0);

        final int functionIdx;
        final int lineNumber;

        private Line(int functionIdx, int lineNumber) {
            this.functionIdx = functionIdx;
            this.lineNumber = lineNumber;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof Line)) return false;

            Line other = (Line) o;
            return functionIdx == other.functionIdx && lineNumber == other.lineNumber;
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + functionIdx;
            return 31 * result + lineNumber;
        }
    }

    private static final class KeyValue {
        static final KeyValue EMPTY = new KeyValue("", "");

        final String key;
        // Only string value is fine for now
        final String value;

        private KeyValue(String key, String value) {
            this.key = key;
            this.value = value;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof KeyValue)) return false;

            KeyValue other = (KeyValue) o;
            return Objects.equals(key, other.key) && Objects.equals(value, other.value);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + key.hashCode();
            return 31 * result + value.hashCode();
        }
    }
}
