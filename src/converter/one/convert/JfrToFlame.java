/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import one.jfr.JfrReader;
import one.jfr.StackTrace;
import one.jfr.event.AllocationSample;
import one.jfr.event.Event;

import java.io.IOException;
import java.io.OutputStream;
import java.io.PrintStream;

import static one.convert.Frame.*;

/**
 * Converts .jfr output to HTML Flame Graph.
 */
public class JfrToFlame extends JfrConverter {
    private final FlameGraph fg;

    public JfrToFlame(JfrReader jfr, Arguments args) {
        super(jfr, args);
        this.fg = new FlameGraph(args);
    }

    @Override
    protected void convertChunk() {
        collector.forEach(new AggregatedEventVisitor() {
            final CallStack stack = new CallStack();

            @Override
            public void visit(Event event, long value) {
                StackTrace stackTrace = jfr.stackTraces.get(event.stackTraceId);
                if (stackTrace != null) {
                    Arguments args = JfrToFlame.this.args;
                    long[] methods = stackTrace.methods;
                    byte[] types = stackTrace.types;
                    int[] locations = stackTrace.locations;

                    if (args.threads) {
                        stack.push(getThreadName(event.tid), TYPE_NATIVE);
                    }
                    if (args.classify) {
                        Classifier.Category category = getCategory(stackTrace);
                        stack.push(category.title, category.type);
                    }
                    for (int i = methods.length; --i >= 0; ) {
                        String methodName = getMethodName(methods[i], types[i]);
                        int location;
                        if (args.lines && (location = locations[i] >>> 16) != 0) {
                            methodName += ":" + location;
                        } else if (args.bci && (location = locations[i] & 0xffff) != 0) {
                            methodName += "@" + location;
                        }
                        stack.push(methodName, types[i]);
                    }
                    long classId = event.classId();
                    if (classId != 0) {
                        stack.push(getClassName(classId), (event instanceof AllocationSample)
                                && ((AllocationSample) event).tlabSize == 0 ? TYPE_KERNEL : TYPE_INLINED);
                    }

                    fg.addSample(stack, value);
                    stack.clear();
                }
            }
        });
    }

    public void dump(OutputStream out) throws IOException {
        fg.dump(out);
    }

    public static FlameGraph parse(String input, Arguments args) throws IOException {
        try (JfrReader jfr = new JfrReader(input)) {
            JfrToFlame converter = new JfrToFlame(jfr, args);
            converter.convert();
            return converter.fg;
        }
    }

    public static void convert(String input, String output, Arguments args) throws IOException {
        FlameGraph fg = parse(input, args);
        try (PrintStream out = new PrintStream(output, "UTF-8")) {
            fg.dump(out);
        }
    }
}
