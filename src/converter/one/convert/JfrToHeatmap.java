/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import one.heatmap.FrameFormatter;
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

public class JfrToHeatmap extends JfrConverter implements FrameFormatter {
    private final Heatmap heatmap;

    public JfrToHeatmap(JfrReader jfr, Arguments args) {
        super(jfr, args);
        this.heatmap = new Heatmap(args, this);
    }

    @Override
    protected EventCollector createCollector(Arguments args) {
        return new EventCollector() {
            @Override
            public void collect(Event e) {
                int extra = 0;
                byte type = 0;
                if (e instanceof AllocationSample) {
                    extra = ((AllocationSample) e).classId;
                    type = ((AllocationSample) e).tlabSize == 0 ? TYPE_KERNEL : TYPE_INLINED;
                } else if (e instanceof ContendedLock) {
                    extra = ((ContendedLock) e).classId;
                    type = TYPE_KERNEL;
                }

                long msFromStart = (e.time - jfr.chunkStartTicks) * 1_000 / jfr.ticksPerSec;
                long timeMs = jfr.chunkStartNanos / 1_000_000 + msFromStart;

                heatmap.addEvent(e.stackTraceId, extra, type, timeMs);
            }

            @Override
            public void beforeChunk() {
                heatmap.assignConstantPool(jfr.methods, jfr.classes, jfr.symbols);
                jfr.stackTraces.forEach(new Dictionary.Visitor<StackTrace>() {
                    @Override
                    public void visit(long key, StackTrace value) {
                        heatmap.addStack(key, value.methods, value.locations, value.types, value.methods.length);
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

    @Override
    public boolean isNativeFrame(byte type) {
        return super.isNativeFrame(type);
    }

    @Override
    public String toJavaClassName(byte[] symbol) {
        return super.toJavaClassName(symbol);
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
