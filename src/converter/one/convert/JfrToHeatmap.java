/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import one.heatmap.Heatmap;
import one.jfr.Dictionary;
import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.AllocationSample;
import one.jfr.event.ContendedLock;
import one.jfr.event.Event;
import one.jfr.event.EventCollector;

import java.io.*;

import static one.convert.Frame.TYPE_INLINED;
import static one.convert.Frame.TYPE_KERNEL;

public class JfrToHeatmap extends JfrConverter {
    private final Heatmap heatmap;

    public JfrToHeatmap(JfrReader jfr, Arguments args) {
        super(jfr, args);
        this.heatmap = new Heatmap(args, this);
    }

    @Override
    protected EventCollector createCollector(Arguments args) {
        return new EventCollector() {
            @Override
            public void collect(Event event) {
                int extra = 0;
                byte type = 0;
                if (event instanceof AllocationSample) {
                    extra = ((AllocationSample) event).classId;
                    type = ((AllocationSample) event).tlabSize == 0 ? TYPE_KERNEL : TYPE_INLINED;
                } else if (event instanceof ContendedLock) {
                    extra = ((ContendedLock) event).classId;
                    type = TYPE_KERNEL;
                }

                long msFromStart = (event.time - jfr.chunkStartTicks) * 1_000 / jfr.ticksPerSec;
                long timeMs = jfr.chunkStartNanos / 1_000_000 + msFromStart;

                heatmap.addEvent(event.stackTraceId, extra, type, timeMs);
            }

            @Override
            public void beforeChunk() {
                heatmap.beforeChunk();
                jfr.stackTraces.forEach(new Dictionary.Visitor<StackTrace>() {
                    @Override
                    public void visit(long key, StackTrace trace) {
                        heatmap.addStack(key, trace.methods, trace.locations, trace.types, trace.methods.length);
                    }
                });
            }

            @Override
            public void afterChunk() {
                jfr.stackTraces.clear();
            }

            @Override
            public boolean finish() {
                heatmap.finish(jfr.startNanos / 1_000_000);
                return false;
            }

            @Override
            public void forEach(Visitor visitor) {
                throw new AssertionError("Should not be called");
            }
        };
    }

    public void dump(OutputStream out) throws IOException {
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
