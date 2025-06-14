/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import static one.convert.OtlpConstants.*;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.Event;
import one.proto.Proto;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

/** Converts .jfr output to OpenTelemetry protocol. */
public class JfrToOtlp extends JfrConverter {
    private final Index<String> stringPool = new Index<>(String.class, "");
    private final Index<String> functionPool = new Index<>(String.class, "");
    private final Index<Line> linePool = new Index<>(Line.class, Line.EMPTY);
    private final Index<KeyValue> attributesPool = new Index<>(KeyValue.class, KeyValue.EMPTY);

    private final Proto otlpProto = new Proto(1024);

    private final int resourceProfilesMark;
    private final int scopeProfilesMark;

    public JfrToOtlp(JfrReader jfr, Arguments args) {
        super(jfr, args);

        resourceProfilesMark = otlpProto.startField(PROFILES_DATA_resource_profiles);
        scopeProfilesMark = otlpProto.startField(RESOURCE_PROFILES_scope_profiles);
    }

    @Override
    protected void convertChunk() {
        int profileMark = otlpProto.startField(SCOPE_PROFILES_profiles);

        writeSampleTypes();
        writeTimingInformation();
        otlpProto.field(PROFILE_original_payload, jfr.currentChunk());

        int locationIndicesMark = otlpProto.startField(PROFILE_location_indices);
        List<SampleInfo> sampleInfos = new ArrayList<>();
        collector.forEach(new OtlpEventVisitor(sampleInfos));
        otlpProto.commitField(locationIndicesMark);

        long framesSeen = 0;
        for (SampleInfo si : sampleInfos) {
            int sampleMark = otlpProto.startField(PROFILE_sample);
            otlpProto.field(SAMPLE_locations_start_index, framesSeen);
            otlpProto.field(SAMPLE_locations_length, si.numFrames);
            otlpProto.field(SAMPLE_timestamps_unix_nano, si.timeNanos);

            KeyValue threadNameAttribute = new KeyValue("thread.name", si.threadName);
            otlpProto.field(SAMPLE_attribute_indices, attributesPool.index(threadNameAttribute));

            int sampleValueMark = otlpProto.startField(SAMPLE_value);
            otlpProto.writeLong(si.samples);
            otlpProto.writeLong(si.value);
            otlpProto.commitField(sampleValueMark);

            otlpProto.commitField(sampleMark);

            framesSeen += si.numFrames;
        }

        otlpProto.commitField(profileMark);
    }

    private void writeSampleTypes() {
        int sampleTypeSamplesMark = otlpProto.startField(PROFILE_sample_type);
        otlpProto.field(VALUE_TYPE_type_strindex, stringPool.index(getValueType()));
        otlpProto.field(VALUE_TYPE_unit_strindex, stringPool.index(getSampleUnits()));
        otlpProto.field(VALUE_TYPE_aggregation_temporality, AGGREGATION_TEMPORARALITY_cumulative);
        otlpProto.commitField(sampleTypeSamplesMark);

        int sampleTypeTotalMark = otlpProto.startField(PROFILE_sample_type);
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
        otlpProto.commitField(scopeProfilesMark);
        otlpProto.commitField(resourceProfilesMark);

        writeProfileDictionary();

        out.write(otlpProto.buffer(), 0, otlpProto.size());
    }

    private void writeProfileDictionary() {
        int profilesDictionaryMark = otlpProto.startField(PROFILES_DATA_dictionary);

        int mappingMark = otlpProto.startField(PROFILES_DICTIONARY_mapping_table);
        otlpProto.commitField(mappingMark);

        // Write function table
        for (String functionName : functionPool.keys()) {
            int functionMark = otlpProto.startField(PROFILES_DICTIONARY_function_table);
            int functionNameStrindex = stringPool.index(functionName);
            otlpProto.field(FUNCTION_name_strindex, functionNameStrindex);
            otlpProto.commitField(functionMark);
        }

        KeyValue frameTypeKv = new KeyValue(FRAME_TYPE_ATTRIBUTE_KEY, "jvm");
        int frameTypeKvAttributeIdx = attributesPool.index(frameTypeKv);

        // Write location table
        for (Line line : linePool.keys()) {
            int locationMark = otlpProto.startField(PROFILES_DICTIONARY_location_table);
            otlpProto.field(LOCATION_mapping_index, 0);
            otlpProto.field(LOCATION_attribute_indices, frameTypeKvAttributeIdx);

            int lineMark = otlpProto.startField(LOCATION_line);
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
            int attributeMark = otlpProto.startField(PROFILES_DICTIONARY_attribute_table);
            otlpProto.field(KEY_VALUE_key, keyValue.key);

            int valueMark = otlpProto.startField(KEY_VALUE_value);
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

    private final class OtlpEventVisitor extends NormalizedEventVisitor {
        private final List<SampleInfo> sampleInfos;

        public OtlpEventVisitor(List<SampleInfo> sampleInfos) {
            this.sampleInfos = sampleInfos;
        }

        @Override
        public void visitImpl(Event event, long samples, long value) {
            StackTrace stackTrace = jfr.stackTraces.get(event.stackTraceId);
            if (stackTrace == null) {
                return;
            }

            long[] methods = stackTrace.methods;
            int[] locations = stackTrace.locations;

            for (int i = methods.length; --i >= 0; ) {
                String methodName = getMethodName(methods[i], stackTrace.types[i]);
                int lineNumber = locations[i] >>> 16;
                int functionIdx = functionPool.index(methodName);

                Line line = new Line(functionIdx, lineNumber);
                otlpProto.writeLong(linePool.index(line));
            }

            long msFromStart = (event.time - jfr.chunkStartTicks) * 1_000 / jfr.ticksPerSec;
            long timeNanos = jfr.chunkStartNanos + msFromStart * 1_000_000;
            sampleInfos.add(
                    new SampleInfo(
                            samples, value, methods.length, getThreadName(event.tid), timeNanos));
        }
    }

    private static final class SampleInfo {
        final long samples;
        final long value;
        final long numFrames;
        final String threadName;
        final long timeNanos;

        public SampleInfo(
                long samples, long value, long numFrames, String threadName, long timeNanos) {
            this.samples = samples;
            this.value = value;
            this.numFrames = numFrames;
            this.threadName = threadName;
            this.timeNanos = timeNanos;
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
            return Objects.hash(functionIdx, lineNumber);
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
            return Objects.hash(key, value);
        }
    }
}
