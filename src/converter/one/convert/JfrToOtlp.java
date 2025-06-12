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

/** Converts .jfr output to OpenTelemetry protocol. */
public class JfrToOtlp extends JfrConverter {
    private final Index<String> stringPool = new Index<>(String.class, "");
    private final Index<String> functionsPool = new Index<>(String.class, "");
    private final Proto otlpProto = new Proto(1024);

    private final int resourceProfilesMark;
    private final int scopeProfilesMark;

    public JfrToOtlp(JfrReader jfr, Arguments args) {
        super(jfr, args);

        resourceProfilesMark = otlpProto.startField(Otlp.ProfilesData.RESOURCE_PROFILES);
        scopeProfilesMark = otlpProto.startField(Otlp.ResourceProfiles.SCOPE_PROFILES);
    }

    @Override
    protected void convertChunk() {
        int profileMark = otlpProto.startField(Otlp.ScopeProfiles.PROFILES);

        writeSampleTypes();
        writeTimingInformation();
        otlpProto.field(Otlp.Profile.ORIGINAL_PAYLOAD, jfr.currentChunk());

        int locationIndicesMark = otlpProto.startField(Otlp.Profile.LOCATION_INDICES);
        List<SampleInfo> sampleInfos = new ArrayList<>();
        collector.forEach(
                new NormalizedEventVisitor() {
                    final CallStack stack = new CallStack();

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
                            int location;
                            if (args.lines && (location = locations[i] >>> 16) != 0) {
                                methodName += ":" + location;
                            } else if (args.bci && (location = locations[i] & 0xffff) != 0) {
                                methodName += "@" + location;
                            }
                            stack.push(methodName, types[i]);
                            otlpProto.writeLong(functionsPool.index(methodName));
                        }

                        sampleInfos.add(new SampleInfo(samples, value, stack.size));
                        stack.clear();
                    }
                });
        otlpProto.commitField(locationIndicesMark);

        long framesSeen = 0;
        for (SampleInfo si : sampleInfos) {
            int sampleMark = otlpProto.startField(Otlp.Profile.SAMPLE);
            otlpProto.field(Otlp.Sample.LOCATIONS_START_INDEX, framesSeen);
            otlpProto.field(Otlp.Sample.LOCATIONS_LENGTH, si.numFrames);

            int sampleValueMark = otlpProto.startField(Otlp.Sample.VALUE);
            otlpProto.writeLong(si.samples);
            otlpProto.writeLong(si.value);
            otlpProto.commitField(sampleValueMark);

            otlpProto.commitField(sampleMark);

            framesSeen += si.numFrames;
        }

        otlpProto.commitField(profileMark);
    }

    private void writeSampleTypes() {
        int sampleTypeSamplesMark = otlpProto.startField(Otlp.Profile.SAMPLE_TYPE);
        otlpProto.field(Otlp.ValueType.TYPE_STRINDEX, stringPool.index(args.getValueType()));
        otlpProto.field(Otlp.ValueType.UNIT_STRINDEX, stringPool.index(args.getSampleUnits()));
        otlpProto.field(
                Otlp.ValueType.AGGREGATION_TEMPORALITY, Otlp.AggregationTemporality.CUMULATIVE);
        otlpProto.commitField(sampleTypeSamplesMark);

        int sampleTypeTotalMark = otlpProto.startField(Otlp.Profile.SAMPLE_TYPE);
        otlpProto.field(Otlp.ValueType.TYPE_STRINDEX, stringPool.index(args.getValueType()));
        otlpProto.field(Otlp.ValueType.UNIT_STRINDEX, stringPool.index(args.getTotalUnits()));
        otlpProto.field(
                Otlp.ValueType.AGGREGATION_TEMPORALITY, Otlp.AggregationTemporality.CUMULATIVE);
        otlpProto.commitField(sampleTypeTotalMark);
    }

    private void writeTimingInformation() {
        otlpProto.field(Otlp.Profile.TIME_NANOS, jfr.getChunkStartNanos());
        otlpProto.field(Otlp.Profile.DURATION_NANOS, jfr.chunkDurationNanos());
    }

    public void dump(OutputStream out) throws IOException {
        otlpProto.commitField(scopeProfilesMark);
        otlpProto.commitField(resourceProfilesMark);

        writeProfileDictionary();

        out.write(otlpProto.buffer());
    }

    private void writeProfileDictionary() {
        int profilesDictionaryMark = otlpProto.startField(Otlp.ProfilesData.DICTIONARY);

        int mappingMark = otlpProto.startField(Otlp.ProfilesDictionary.MAPPING_TABLE);
        otlpProto.commitField(mappingMark);

        // Write function table
        for (String functionName : functionsPool.keys()) {
            int functionMark = otlpProto.startField(Otlp.ProfilesDictionary.FUNCTION_TABLE);
            otlpProto.field(Otlp.Function.NAME_STRINDEX, stringPool.index(functionName));
            otlpProto.commitField(functionMark);
        }

        // Write location table
        for (long functionIdx = 0; functionIdx < functionsPool.size(); ++functionIdx) {
            int locationMark = otlpProto.startField(Otlp.ProfilesDictionary.LOCATION_TABLE);
            otlpProto.field(Otlp.Location.MAPPING_INDEX, 0);
            int lineMark = otlpProto.startField(Otlp.Location.LINE);
            otlpProto.field(Otlp.Line.FUNCTION_INDEX, functionIdx);
            otlpProto.commitField(lineMark);
            otlpProto.commitField(locationMark);
        }

        // Write string table
        for (String s : stringPool.keys()) {
            otlpProto.field(Otlp.ProfilesDictionary.STRING_TABLE, s);
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
}
