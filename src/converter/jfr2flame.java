/*
 * Copyright 2020 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import one.jfr.ClassRef;
import one.jfr.Dictionary;
import one.jfr.JfrReader;
import one.jfr.MethodRef;
import one.jfr.StackTrace;
import one.jfr.event.*;

import java.io.IOException;
import java.nio.charset.StandardCharsets;

/**
 * Converts .jfr output produced by async-profiler to HTML Flame Graph.
 */
public class jfr2flame {

    private static final String[] FRAME_SUFFIX = {"_[0]", "_[j]", "_[i]", "", "", "_[k]", "_[1]"};

    private final JfrReader jfr;
    private final Arguments args;

    public jfr2flame(JfrReader jfr, Arguments args) {
        this.jfr = jfr;
        this.args = args;
    }

    public void convert(final FlameGraph fg) throws IOException {
        EventAggregator agg = new EventAggregator(args.threads, args.total);

        Class<? extends Event> eventClass =
                args.live ? LiveObject.class :
                        args.alloc ? AllocationSample.class :
                                args.lock ? ContendedLock.class : ExecutionSample.class;

        long threadStates = 0;
        if (args.state != null) {
            for (String state : args.state.split(",")) {
                int key = jfr.getEnumKey("jdk.types.ThreadState", "STATE_" + state.toUpperCase());
                if (key >= 0) threadStates |= 1L << key;
            }
        }

        long startTicks = args.from != 0 ? toTicks(args.from) : Long.MIN_VALUE;
        long endTicks = args.to != 0 ? toTicks(args.to) : Long.MAX_VALUE;

        for (Event event; (event = jfr.readEvent(eventClass)) != null; ) {
            if (event.time >= startTicks && event.time <= endTicks) {
                if (threadStates == 0 || (threadStates & (1L << ((ExecutionSample) event).threadState)) != 0) {
                    agg.collect(event);
                }
            }
        }

        final double ticksToNanos = 1e9 / jfr.ticksPerSec;
        final boolean scale = args.total && args.lock && ticksToNanos != 1.0;
        final Classifier classifier = new Classifier(jfr.methodNames);

        // Don't use lambda for faster startup
        agg.forEach(new EventAggregator.Visitor() {
            @Override
            public void visit(Event event, long value) {
                StackTrace stackTrace = jfr.stackTraces.get(event.stackTraceId);
                if (stackTrace != null) {
                    Arguments args = jfr2flame.this.args;
                    long[] methods = stackTrace.methods;
                    byte[] types = stackTrace.types;
                    int[] locations = stackTrace.locations;
                    String classFrame = getClassFrame(event);
                    String[] trace = new String[methods.length
                            + (args.threads ? 1 : 0)
                            + (args.classify ? 1 : 0)
                            + (classFrame != null ? 1 : 0)];
                    if (args.threads) {
                        trace[0] = getThreadFrame(event.tid);
                    }
                    int idx = trace.length;
                    if (classFrame != null) {
                        trace[--idx] = classFrame;
                    }
                    for (int i = 0; i < methods.length; i++) {
                        String methodName = getMethodName(methods[i], types[i]);
                        int location;
                        if (args.lines && (location = locations[i] >>> 16) != 0) {
                            methodName += ":" + location;
                        } else if (args.bci && (location = locations[i] & 0xffff) != 0) {
                            methodName += "@" + location;
                        }
                        trace[--idx] = methodName + FRAME_SUFFIX[types[i]];
                    }
                    if (args.classify) {
                        trace[--idx] = classifier.getCategoryName(stackTrace);
                    }
                    fg.addSample(trace, scale ? (long) (value * ticksToNanos) : value);
                }
            }
        });
    }

    private String getThreadFrame(int tid) {
        String threadName = jfr.threads.get(tid);
        return threadName == null ? "[tid=" + tid + ']' :
                threadName.startsWith("[tid=") ? threadName : '[' + threadName + " tid=" + tid + ']';
    }

    private String getClassFrame(Event event) {
        long classId;
        String suffix;
        if (event instanceof AllocationSample) {
            classId = ((AllocationSample) event).classId;
            suffix = ((AllocationSample) event).tlabSize == 0 ? "_[k]" : "_[i]";
        } else if (event instanceof ContendedLock) {
            classId = ((ContendedLock) event).classId;
            suffix = "_[i]";
        } else if (event instanceof LiveObject) {
            classId = ((LiveObject) event).classId;
            suffix = "_[i]";
        } else {
            return null;
        }

        ClassRef cls = jfr.classes.get(classId);
        if (cls == null) {
            return "null";
        }
        byte[] className = jfr.symbols.get(cls.name);

        int arrayDepth = 0;
        while (className[arrayDepth] == '[') {
            arrayDepth++;
        }

        StringBuilder sb = new StringBuilder(toJavaClassName(className, arrayDepth, true));
        while (arrayDepth-- > 0) {
            sb.append("[]");
        }
        return sb.append(suffix).toString();
    }

    private String getMethodName(long methodId, byte methodType) {
        return jfr.getMethodName(methodId, methodType, this.args.dot);
    }

    private boolean isNativeFrame(byte methodType) {
        return methodType == FlameGraph.FRAME_NATIVE && jfr.getEnumValue("jdk.types.FrameType", FlameGraph.FRAME_KERNEL) != null
                || methodType == FlameGraph.FRAME_CPP
                || methodType == FlameGraph.FRAME_KERNEL;
    }

    private String toJavaClassName(byte[] symbol, int start, boolean dotted) {
        return jfr.toJavaClassName(symbol, start, dotted, this.args.simple);
    }

    // millis can be an absolute timestamp or an offset from the beginning/end of the recording
    private long toTicks(long millis) {
        long nanos = millis * 1_000_000;
        if (millis < 0) {
            nanos += jfr.endNanos;
        } else if (millis < 1500000000000L) {
            nanos += jfr.startNanos;
        }
        return jfr.nanosToTicks(nanos);
    }

    public static void main(String[] cmdline) throws Exception {
        Arguments args = new Arguments(cmdline);
        if (args.input == null) {
            System.out.println("Usage: java " + jfr2flame.class.getName() + " [options] input.jfr [output.html]");
            System.out.println();
            System.out.println("options include all supported FlameGraph options, plus the following:");
            System.out.println("  --alloc       Allocation Flame Graph");
            System.out.println("  --live        Include only live objects in allocation profile");
            System.out.println("  --lock        Lock contention Flame Graph");
            System.out.println("  --threads     Split profile by threads");
            System.out.println("  --state LIST  Filter samples by thread states: RUNNABLE, SLEEPING, etc.");
            System.out.println("  --classify    Classify samples into predefined categories");
            System.out.println("  --total       Accumulate the total value (time, bytes, etc.)");
            System.out.println("  --lines       Show line numbers");
            System.out.println("  --bci         Show bytecode indices");
            System.out.println("  --simple      Simple class names instead of FQN");
            System.out.println("  --dot         Dotted class names");
            System.out.println("  --from TIME   Start time in ms (absolute or relative)");
            System.out.println("  --to TIME     End time in ms (absolute or relative)");
            System.out.println("  --collapsed   Use collapsed stacks output format");
            System.exit(1);
        }

        boolean collapsed = args.collapsed || args.output != null && args.output.endsWith(".collapsed");
        FlameGraph fg = collapsed ? new CollapsedStacks(args) : new FlameGraph(args);

        try (JfrReader jfr = new JfrReader(args.input)) {
            new jfr2flame(jfr, args).convert(fg);
        }

        fg.dump();
    }
}
