/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

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
    private final Index<Function> functionPool = new Index<>(Function.class, null);
    private final Proto otlpProto = new Proto(1024);

    private final int resourceProfilesMark;
    private final int scopeProfilesMark;

    public JfrToOtlp(JfrReader jfr, Arguments args) {
        super(jfr, args);

        resourceProfilesMark = otlpProto.startField(Otlp.ProfilesData.RESOURCE_PROFILES.index);
        scopeProfilesMark = otlpProto.startField(Otlp.ResourceProfiles.SCOPE_PROFILES.index);
    }

    @Override
    protected void convertChunk() {
        int profileMark = otlpProto.startField(Otlp.ScopeProfiles.PROFILES.index);

        writeSampleTypes();
        writeTimingInformation();
        otlpProto.field(Otlp.Profile.ORIGINAL_PAYLOAD.index, jfr.currentChunk());

        int locationIndicesMark = otlpProto.startField(Otlp.Profile.LOCATION_INDICES.index);
        List<SampleInfo> sampleInfos = new ArrayList<>();
        collector.forEach(new OtlpEventVisitor(sampleInfos));
        otlpProto.commitField(locationIndicesMark);

        long framesSeen = 0;
        for (SampleInfo si : sampleInfos) {
            int sampleMark = otlpProto.startField(Otlp.Profile.SAMPLE.index);
            otlpProto.field(Otlp.Sample.LOCATIONS_START_INDEX.index, framesSeen);
            otlpProto.field(Otlp.Sample.LOCATIONS_LENGTH.index, si.numFrames);

            int sampleValueMark = otlpProto.startField(Otlp.Sample.VALUE.index);
            otlpProto.writeLong(si.samples);
            otlpProto.writeLong(si.value);
            otlpProto.commitField(sampleValueMark);

            otlpProto.commitField(sampleMark);

            framesSeen += si.numFrames;
        }

        otlpProto.commitField(profileMark);
    }

    private void writeSampleTypes() {
        int sampleTypeSamplesMark = otlpProto.startField(Otlp.Profile.SAMPLE_TYPE.index);
        otlpProto.field(Otlp.ValueType.TYPE_STRINDEX.index, stringPool.index(getValueType()));
        otlpProto.field(Otlp.ValueType.UNIT_STRINDEX.index, stringPool.index(getSampleUnits()));
        otlpProto.field(
                Otlp.ValueType.AGGREGATION_TEMPORALITY.index,
                Otlp.AggregationTemporality.CUMULATIVE.value);
        otlpProto.commitField(sampleTypeSamplesMark);

        int sampleTypeTotalMark = otlpProto.startField(Otlp.Profile.SAMPLE_TYPE.index);
        otlpProto.field(Otlp.ValueType.TYPE_STRINDEX.index, stringPool.index(getValueType()));
        otlpProto.field(Otlp.ValueType.UNIT_STRINDEX.index, stringPool.index(getTotalUnits()));
        otlpProto.field(
                Otlp.ValueType.AGGREGATION_TEMPORALITY.index,
                Otlp.AggregationTemporality.CUMULATIVE.value);
        otlpProto.commitField(sampleTypeTotalMark);
    }

    private void writeTimingInformation() {
        otlpProto.field(Otlp.Profile.TIME_NANOS.index, jfr.getChunkStartNanos());
        otlpProto.field(Otlp.Profile.DURATION_NANOS.index, jfr.chunkDurationNanos());
    }

    public void dump(OutputStream out) throws IOException {
        otlpProto.commitField(scopeProfilesMark);
        otlpProto.commitField(resourceProfilesMark);

        writeProfileDictionary();

        out.write(otlpProto.buffer(), 0, otlpProto.size());
    }

    private void writeProfileDictionary() {
        int profilesDictionaryMark = otlpProto.startField(Otlp.ProfilesData.DICTIONARY.index);

        int mappingMark = otlpProto.startField(Otlp.ProfilesDictionary.MAPPING_TABLE.index);
        otlpProto.commitField(mappingMark);

        Function[] orderedFunctions = functionPool.keys();

        // Write function table
        for (Function function : orderedFunctions) {
            int functionMark = otlpProto.startField(Otlp.ProfilesDictionary.FUNCTION_TABLE.index);
            int functionNameStrindex = stringPool.index(function.functionName);
            otlpProto.field(Otlp.Function.NAME_STRINDEX.index, functionNameStrindex);
            otlpProto.commitField(functionMark);
        }

        // Write location table
        for (int functionIdx = 0; functionIdx < orderedFunctions.length; ++functionIdx) {
            int locationMark = otlpProto.startField(Otlp.ProfilesDictionary.LOCATION_TABLE.index);
            otlpProto.field(Otlp.Location.MAPPING_INDEX.index, 0);

            int lineMark = otlpProto.startField(Otlp.Location.LINE.index);
            otlpProto.field(Otlp.Line.FUNCTION_INDEX.index, functionIdx);
            otlpProto.field(Otlp.Line.LINE.index, orderedFunctions[functionIdx].lineNumber);
            otlpProto.commitField(lineMark);

            otlpProto.commitField(locationMark);
        }

        // Write string table
        for (String s : stringPool.keys()) {
            otlpProto.field(Otlp.ProfilesDictionary.STRING_TABLE.index, s);
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

            Arguments args = JfrToOtlp.this.args;
            long[] methods = stackTrace.methods;
            byte[] types = stackTrace.types;
            int[] locations = stackTrace.locations;

            for (int i = methods.length; --i >= 0; ) {
                String methodName = getMethodName(methods[i], types[i]);
                int lineNumber = locations[i] >>> 16;
                otlpProto.writeLong(functionPool.index(new Function(methodName, lineNumber)));
            }

            sampleInfos.add(new SampleInfo(samples, value, methods.length));
        }
    }

    private static final class SampleInfo {
        public final long samples;
        public final long value;
        public final long numFrames;

        public SampleInfo(long samples, long value, long numFrames) {
            this.samples = samples;
            this.value = value;
            this.numFrames = numFrames;
        }
    }

    private static final class Function {
        private final String functionName;
        private final int lineNumber;

        private Function(String functionName, int lineNumber) {
            this.functionName = functionName;
            this.lineNumber = lineNumber;
        }

        @Override
        public boolean equals(Object obj) {
            if (!(obj instanceof Function)) {
                return false;
            }

            Function other = (Function) obj;
            return Objects.equals(functionName, other.functionName)
                    && lineNumber == other.lineNumber;
        }

        @Override
        public int hashCode() {
            return Objects.hash(functionName, lineNumber);
        }
    }
}
