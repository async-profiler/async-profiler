/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import static one.convert.Frame.TYPE_INLINED;
import static one.convert.Frame.TYPE_KERNEL;
import static one.convert.Frame.TYPE_NATIVE;

import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.util.concurrent.TimeUnit;
import one.heatmap.Heatmap;
import one.jfr.Dictionary;
import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.AllocationSample;
import one.jfr.event.ContendedLock;
import one.jfr.event.Event;
import one.jfr.event.EventAggregator;

public class JfrToHeatmap extends JfrConverter {

    private final Heatmap heatmap;

    public JfrToHeatmap(JfrReader jfr, Arguments args) {
        super(jfr, args);
        this.heatmap = new Heatmap(args);
    }

    @Override
    protected void convertChunk() throws IOException {
        heatmap.assignConstantPool(jfr.methods, jfr.classes, jfr.symbols, jfr.isAsyncProfiler());
        jfr.stackTraces.forEach(new Dictionary.Visitor<StackTrace>() {
            @Override
            public void visit(long key, StackTrace value) {
                heatmap.addStack(key, value.methods, value.locations, value.types, value.methods.length);
            }
        });
        collectEvents().forEach(new EventAggregator.Visitor() {

            @Override
            public void visit(Event event, long value) {
                int extra = 0;
                byte type = 0;
                if (event instanceof AllocationSample) {
                    extra = ((AllocationSample) event).classId;
                    type = ((AllocationSample) event).tlabSize == 0 ? TYPE_KERNEL : TYPE_INLINED;
                } else if (event instanceof ContendedLock) {
                    extra = ((ContendedLock) event).classId;
                    type = TYPE_KERNEL;
                }

                long msFromStart = (event.time - jfr.chunkStartTicks) * 1_000L / jfr.ticksPerSec;
                long timeMs = TimeUnit.NANOSECONDS.toMillis(jfr.chunkStartNanos) + msFromStart;

                heatmap.addEvent(event.stackTraceId, extra, type, timeMs);
            }
        });
        jfr.stackTraces.clear();
    }

    public void dump(OutputStream out) throws IOException {
        heatmap.finish(TimeUnit.NANOSECONDS.toMillis(jfr.startNanos));
        try (PrintStream ps = new PrintStream(out, false, "UTF-8")) {
            heatmap.dump(ps);
        }
    }

    public static void convert(String input, String output, Arguments args) throws IOException {
        JfrToHeatmap converter;
        try (JfrReader jfr = new JfrReader(input)) {
            converter = new JfrToHeatmap(jfr, args);
            converter.convert();
        }
        try (OutputStream out = new BufferedOutputStream(new FileOutputStream(output))) {
            converter.dump(out);
        }
    }
}
