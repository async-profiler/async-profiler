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
            long wallInterval;

            private long getWallInterval() {
                if (wallInterval == 0) {
                    String wall = jfr.settings.get("wall");
                    long interval = Long.parseLong(wall != null ? wall : jfr.settings.get("interval"));
                    wallInterval = interval != 0 ? interval : 50_000_000;
                }
                return wallInterval;
            }

            @Override
            public void collect(Event event) {
                int classId = 0;
                byte type = 0;
                if (event instanceof AllocationSample) {
                    classId = ((AllocationSample) event).classId;
                    type = ((AllocationSample) event).tlabSize == 0 ? TYPE_KERNEL : TYPE_INLINED;
                } else if (event instanceof ContendedLock) {
                    classId = ((ContendedLock) event).classId;
                    type = TYPE_KERNEL;
                }

                long timeNs = jfr.eventTimeToNanos(event.time);
                long samples = event.samples();
                while (true) {
                    heatmap.addEvent(event.stackTraceId, event.tid, classId, type, timeNs / 1_000_000);
                    if (--samples <= 0) break;
                    // Only wall clock events can have samples > 1
                    timeNs += getWallInterval();
                }
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
                wallInterval = 0;
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
