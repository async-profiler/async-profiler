/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.Event;
import one.jfr.event.EventAggregator;
import one.proto.Proto;

import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.util.zip.GZIPOutputStream;

/**
 * Converts .jfr output to <a href="https://github.com/google/pprof">pprof</a>.
 */
public class JfrToPprof extends JfrConverter {
    private final Proto profile = new Proto(100000);
    private final Index<String> strings = new Index<>(String.class, "");
    private final Index<String> functions = new Index<>(String.class, "");
    private final Index<Long> locations = new Index<>(Long.class, 0L);

    public JfrToPprof(JfrReader jfr, Arguments args) {
        super(jfr, args);

        Proto sampleType;
        if (args.alloc || args.live) {
            sampleType = valueType("allocations", args.total ? "bytes" : "count");
        } else if (args.lock) {
            sampleType = valueType("locks", args.total ? "nanoseconds" : "count");
        } else {
            sampleType = valueType("cpu", args.total ? "nanoseconds" : "count");
        }

        profile.field(1, sampleType)
                .field(13, strings.index("Produced by async-profiler"));
    }

    @Override
    protected void convertChunk() throws IOException {
        collectEvents().forEach(new EventAggregator.ValueVisitor() {
            final Proto s = new Proto(100);

            @Override
            public void visit(Event event, long value) {
                profile.field(2, sample(s, event, value));
                s.reset();
            }
        });
    }

    public void dump(OutputStream out) throws IOException {
        profile.field(3, mapping(1, 0, Long.MAX_VALUE, "async-profiler"));

        Long[] locations = this.locations.keys();
        for (int i = 1; i < locations.length; i++) {
            profile.field(4, location(i, locations[i]));
        }

        String[] functions = this.functions.keys();
        for (int i = 1; i < functions.length; i++) {
            profile.field(5, function(i, functions[i]));
        }

        String[] strings = this.strings.keys();
        for (String string : strings) {
            profile.field(6, string);
        }

        profile.field(9, jfr.startNanos)
                .field(10, jfr.durationNanos());

        out.write(profile.buffer(), 0, profile.size());
    }

    private Proto sample(Proto s, Event event, long value) {
        int packedLocations = s.startField(1);

        long classId = event.classId();
        if (classId != 0) {
            int function = functions.index(getClassName(classId));
            s.writeInt(locations.index((long) function << 16));
        }

        StackTrace stackTrace = jfr.stackTraces.get(event.stackTraceId);
        if (stackTrace != null) {
            long[] methods = stackTrace.methods;
            byte[] types = stackTrace.types;
            int[] lines = stackTrace.locations;
            for (int i = 0; i < methods.length; i++) {
                String methodName = getMethodName(methods[i], types[i]);
                int function = functions.index(methodName);
                s.writeInt(locations.index((long) function << 16 | lines[i] >>> 16));
            }
        }

        s.commitField(packedLocations);
        s.field(2, value);

        if (args.threads && event.tid != 0) {
            s.field(3, label("thread", getThreadName(event.tid)));
        }
        if (args.classify && stackTrace != null) {
            s.field(3, label("category", getCategory(stackTrace).title));
        }

        return s;
    }

    private Proto valueType(String type, String unit) {
        return new Proto(16)
                .field(1, strings.index(type))
                .field(2, strings.index(unit));
    }

    private Proto label(String key, String str) {
        return new Proto(16)
                .field(1, strings.index(key))
                .field(2, strings.index(str));
    }

    private Proto mapping(int id, long start, long limit, String fileName) {
        return new Proto(16)
                .field(1, id)
                .field(2, start)
                .field(3, limit)
                .field(5, strings.index(fileName));
    }

    private Proto location(int id, long location) {
        return new Proto(16)
                .field(1, id)
                .field(4, line((int) (location >>> 16), (int) location & 0xffff));
    }

    private Proto line(int functionId, int line) {
        return new Proto(16)
                .field(1, functionId)
                .field(2, line);
    }

    private Proto function(int id, String name) {
        return new Proto(16)
                .field(1, id)
                .field(2, strings.index(name));
    }

    public static void convert(String input, String output, Arguments args) throws IOException {
        JfrToPprof converter;
        try (JfrReader jfr = new JfrReader(input)) {
            converter = new JfrToPprof(jfr, args);
            converter.convert();
        }
        try (FileOutputStream fos = new FileOutputStream(output);
             OutputStream out = args.output.endsWith("gz") ? new GZIPOutputStream(fos, 4096) : fos) {
            converter.dump(out);
        }
    }
}
