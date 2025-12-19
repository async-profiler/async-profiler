/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import static one.convert.OtlpConstants.*;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.Event;
import one.jfr.event.EventCollector;
import one.proto.Proto;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.*;

/** Converts .jfr output to OpenTelemetry protocol. */
public class JfrToOtlp extends JfrConverter {
    // Size in bytes to be allocated in the buffer to hold the varint containing the length of the message
    private static final int MSG_LARGE = 5;
    private static final int MSG_SMALL = 1;

    private final Index<String> stringPool = new Index<>(String.class, "");
    private final Index<String> functionPool = new Index<>(String.class, "");
    private final Index<Line> linePool = new Index<>(Line.class, Line.EMPTY);
    private final Index<KeyValue> attributesPool = new Index<>(KeyValue.class, KeyValue.EMPTY);
    private final Index<IntArrayWrapper> stacksPool = new Index<>(IntArrayWrapper.class, IntArrayWrapper.EMPTY);
    private final int threadNameIndex = stringPool.index(OTLP_THREAD_NAME);

    private final Proto proto = new Proto(1024);

    public JfrToOtlp(JfrReader jfr, Arguments args) {
        super(jfr, args);
    }

    public void dump(OutputStream out) throws IOException {
        out.write(proto.buffer(), 0, proto.size());
    }

    @Override
    public void convert() throws IOException {
        long rpMark = proto.startField(PROFILES_DATA_resource_profiles, MSG_LARGE);
        long spMark = proto.startField(RESOURCE_PROFILES_scope_profiles, MSG_LARGE);
        super.convert();
        proto.commitField(spMark);
        proto.commitField(rpMark);

        writeProfileDictionary();
    }

    @Override
    protected void convertChunk() {
        List<SampleInfo> samplesInfo = new ArrayList<>();
        collector.forEach(new OtlpEventToSampleVisitor(samplesInfo));

        { // Counter profile
            long pMark = proto.startField(SCOPE_PROFILES_profiles, MSG_LARGE);

            long stsMark = proto.startField(PROFILE_sample_type, MSG_SMALL);
            proto.field(VALUE_TYPE_type_strindex, stringPool.index(getValueType()));
            proto.field(VALUE_TYPE_unit_strindex, stringPool.index(getSampleUnits()));
            proto.commitField(stsMark);

            proto.field(PROFILE_time_unix_nano, jfr.chunkStartNanos);
            proto.field(PROFILE_duration_nanos, jfr.chunkDurationNanos());

            writeSamples(samplesInfo, true /* samples */);

            proto.commitField(pMark);
        }

        { // Total profile
            long pMark = proto.startField(SCOPE_PROFILES_profiles, MSG_LARGE);

            long sttMark = proto.startField(PROFILE_sample_type, MSG_SMALL);
            proto.field(VALUE_TYPE_type_strindex, stringPool.index(getValueType()));
            proto.field(VALUE_TYPE_unit_strindex, stringPool.index(getTotalUnits()));
            proto.commitField(sttMark);

            proto.field(PROFILE_time_unix_nano, jfr.chunkStartNanos);
            proto.field(PROFILE_duration_nanos, jfr.chunkDurationNanos());

            writeSamples(samplesInfo, false /* samples */);

            proto.commitField(pMark);
        }
    }

    private void writeSamples(List<SampleInfo> samplesInfo, boolean samples) {
        for (SampleInfo si : samplesInfo) {
            long sMark = proto.startField(PROFILE_samples, MSG_SMALL);
            proto.field(SAMPLE_stack_index, si.stackIndex);
            proto.field(SAMPLE_values, samples ? si.samples : si.value);
            proto.field(SAMPLE_attribute_indices, si.threadNameAttributeIndex);
            proto.field(SAMPLE_timestamps_unix_nano, si.timeNanos);
            proto.commitField(sMark);
        }
    }

    private void writeProfileDictionary() {
        long profilesDictionaryMark = proto.startField(PROFILES_DATA_dictionary, MSG_LARGE);

        // Mapping[0] must be a default mapping according to the spec
        long mMark = proto.startField(PROFILES_DICTIONARY_mapping_table, MSG_SMALL);
        proto.commitField(mMark);

        for (String name : functionPool.keys()) {
            long fMark = proto.startField(PROFILES_DICTIONARY_function_table, MSG_SMALL);
            proto.field(FUNCTION_name_strindex, stringPool.index(name));
            proto.commitField(fMark);
        }

        for (Line line : linePool.keys()) {
            long locMark = proto.startField(PROFILES_DICTIONARY_location_table, MSG_SMALL);
            proto.field(LOCATION_mapping_index, 0);

            long lineMark = proto.startField(LOCATION_line, MSG_SMALL);
            proto.field(LINE_function_index, line.functionIdx);
            proto.field(LINE_lines, line.lineNumber);
            proto.commitField(lineMark);

            proto.commitField(locMark);
        }

        for (IntArrayWrapper stack : stacksPool.keys()) {
            long stackMark = proto.startField(PROFILES_DICTIONARY_stack_table, MSG_LARGE);
            long locationIndicesMark = proto.startField(STACK_location_indices, MSG_LARGE);
            for (int locationIdx : stack.array) {
                proto.writeInt(locationIdx);
            }
            proto.commitField(locationIndicesMark);
            proto.commitField(stackMark);
        }

        for (String s : stringPool.keys()) {
            proto.field(PROFILES_DICTIONARY_string_table, s);
        }

        for (KeyValue kv : attributesPool.keys()) {
            long aMark = proto.startField(PROFILES_DICTIONARY_attribute_table, MSG_LARGE);
            proto.field(KEY_VALUE_AND_UNIT_key_strindex, kv.keyStrindex);

            long vMark = proto.startField(KEY_VALUE_AND_UNIT_value, MSG_LARGE);
            proto.field(ANY_VALUE_string_value, kv.value);
            proto.commitField(vMark);

            proto.commitField(aMark);
        }

        proto.commitField(profilesDictionaryMark);
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

    private static final class SampleInfo {
        private final long timeNanos;
        private final int threadNameAttributeIndex;
        private final int stackIndex;
        private final long samples;
        private final long value;

        public SampleInfo(long timeNanos, int threadNameAttributeIndex, int stackIndex, long samples, long value) {
            this.timeNanos = timeNanos;
            this.threadNameAttributeIndex = threadNameAttributeIndex;
            this.stackIndex = stackIndex;
            this.samples = samples;
            this.value = value;
        }
    }

    private final class OtlpEventToSampleVisitor implements EventCollector.Visitor {
        private final List<SampleInfo> samplesInfo;
        // Chunk-private cache to remember mappings from stacktrace ID to OTLP stack index
        private final Map<Integer, Integer> stacksIndexCache = new HashMap<>();
        private final double factor = counterFactor();

        public OtlpEventToSampleVisitor(List<SampleInfo> samplesInfo) {
            this.samplesInfo = samplesInfo;
        }

        @Override
        public void visit(Event event, long samples, long value) {
            String threadName = getThreadName(event.tid);
            KeyValue threadNameKv = new KeyValue(threadNameIndex, threadName);
            int stackIndex = stacksIndexCache.computeIfAbsent(event.stackTraceId, key -> stacksPool.index(makeStack(key)));
            long nanosFromStart = (long) ((event.time - jfr.chunkStartTicks) * jfr.nanosPerTick);
            long timeNanos = jfr.chunkStartNanos + nanosFromStart;
            SampleInfo si = new SampleInfo(timeNanos, attributesPool.index(threadNameKv), stackIndex, samples,
                                           factor == 1.0 ? value : (long) (value * factor));
            samplesInfo.add(si);
        }

        private IntArrayWrapper makeStack(int stackTraceId) {
            StackTrace st = jfr.stackTraces.get(stackTraceId);
            int[] stack = new int[st.methods.length];
            for (int i = 0; i < st.methods.length; ++i) {
                stack[i] = linePool.index(makeLine(st, i));
            }
            return new IntArrayWrapper(stack);
        }

        private Line makeLine(StackTrace stackTrace, int i) {
            String methodName = getMethodName(stackTrace.methods[i], stackTrace.types[i]);
            int lineNumber = stackTrace.locations[i] >>> 16;
            int functionIdx = functionPool.index(methodName);
            return new Line(functionIdx, lineNumber);
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
        static final KeyValue EMPTY = new KeyValue(0, "");

        final int keyStrindex;
        // Only string value is fine for now
        final String value;

        private KeyValue(int keyStrindex, String value) {
            this.keyStrindex = keyStrindex;
            this.value = value;
        }

        @Override
        public boolean equals(Object o) {
            if (!(o instanceof KeyValue)) return false;

            KeyValue other = (KeyValue) o;
            return keyStrindex == other.keyStrindex && value.equals(other.value);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + keyStrindex;
            return 31 * result + value.hashCode();
        }
    }

    private static final class IntArrayWrapper {
        static final IntArrayWrapper EMPTY = new IntArrayWrapper(new int[0]);

        final int[] array;
        final int hash;

        IntArrayWrapper(int[] array) {
            this.array = array;
            this.hash = Arrays.hashCode(array);
        }

        @Override
        public boolean equals(Object o) {
            return o instanceof IntArrayWrapper && Arrays.equals(array, ((IntArrayWrapper) o).array);
        }

        @Override
        public int hashCode() {
            return hash;
        }
    }
}
