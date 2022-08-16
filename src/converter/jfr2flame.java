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
import one.jfr.event.AllocationSample;
import one.jfr.event.ContendedLock;
import one.jfr.event.Event;
import one.jfr.event.EventAggregator;
import one.jfr.event.ExecutionSample;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.Map;

/**
 * Converts .jfr output produced by async-profiler to HTML Flame Graph.
 */
public class jfr2flame {

    private static final String[] FRAME_SUFFIX = {"_[0]", "_[j]", "_[i]", "", "", "_[k]", "_[1]"};

    private final JfrReader jfr;
    private final Arguments args;
    private final Dictionary<String> methodNames = new Dictionary<>();

    public jfr2flame(JfrReader jfr, Arguments args) {
        this.jfr = jfr;
        this.args = args;
    }

    public void convert(final FlameGraph fg) throws IOException {
        EventAggregator agg = new EventAggregator(args.threads, args.total);

        Class<? extends Event> eventClass = args.alloc ? AllocationSample.class :
                args.lock ? ContendedLock.class : ExecutionSample.class;
        int threadState = args.cpu ? getMapKey(jfr.threadStates, "STATE_RUNNABLE") : -1;

        long startTicks = args.from != 0 ? toTicks(args.from) : Long.MIN_VALUE;
        long endTicks = args.to != 0 ? toTicks(args.to) : Long.MAX_VALUE;

        for (Event event; (event = jfr.readEvent(eventClass)) != null; ) {
            if (event.time >= startTicks && event.time <= endTicks) {
                if (threadState < 0 || ((ExecutionSample) event).threadState == threadState) {
                    agg.collect(event);
                }
            }
        }

        final double ticksToNanos = 1e9 / jfr.ticksPerSec;
        final boolean scale = args.total && eventClass == ContendedLock.class && ticksToNanos != 1.0;

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
                    String[] trace = new String[methods.length + (args.threads ? 1 : 0) + (classFrame != null ? 1 : 0)];
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
                    fg.addSample(trace, scale ? (long) (value * ticksToNanos) : value);
                }
            }
        });
    }

    private String getThreadFrame(int tid) {
        String threadName = jfr.threads.get(tid);
        return threadName == null ? "[tid=" + tid + ']' : '[' + threadName + " tid=" + tid + ']';
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
        String result = methodNames.get(methodId);
        if (result != null) {
            return result;
        }

        MethodRef method = jfr.methods.get(methodId);
        if (method == null) {
            result = "unknown";
        } else {
            ClassRef cls = jfr.classes.get(method.cls);
            byte[] className = jfr.symbols.get(cls.name);
            byte[] methodName = jfr.symbols.get(method.name);

            if (className == null || className.length == 0 || isNativeFrame(methodType)) {
                result = new String(methodName, StandardCharsets.UTF_8);
            } else {
                String classStr = toJavaClassName(className, 0, args.dot);
                String methodStr = new String(methodName, StandardCharsets.UTF_8);
                result = classStr + '.' + methodStr;
            }
        }

        methodNames.put(methodId, result);
        return result;
    }

    private boolean isNativeFrame(byte methodType) {
        return methodType >= FlameGraph.FRAME_NATIVE && methodType <= FlameGraph.FRAME_KERNEL
                && jfr.frameTypes.size() > FlameGraph.FRAME_NATIVE + 1;
    }

    private String toJavaClassName(byte[] symbol, int start, boolean dotted) {
        int end = symbol.length;
        if (start > 0) {
            switch (symbol[start]) {
                case 'B':
                    return "byte";
                case 'C':
                    return "char";
                case 'S':
                    return "short";
                case 'I':
                    return "int";
                case 'J':
                    return "long";
                case 'Z':
                    return "boolean";
                case 'F':
                    return "float";
                case 'D':
                    return "double";
                case 'L':
                    start++;
                    end--;
            }
        }

        if (args.simple) {
            for (int i = end - 2; i >= start; i--) {
                if (symbol[i] == '/' && (symbol[i + 1] < '0' || symbol[i + 1] > '9')) {
                    start = i + 1;
                    break;
                }
            }
        }

        String s = new String(symbol, start, end - start, StandardCharsets.UTF_8);
        return dotted ? s.replace('/', '.') : s;
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

    private static int getMapKey(Map<Integer, String> map, String value) {
        for (Map.Entry<Integer, String> entry : map.entrySet()) {
            if (value.equals(entry.getValue())) {
                return entry.getKey();
            }
        }
        return -1;
    }

    public static void main(String[] cmdline) throws Exception {
        Arguments args = new Arguments(cmdline);
        if (args.input == null) {
            System.out.println("Usage: java " + jfr2flame.class.getName() + " [options] input.jfr [output.html]");
            System.out.println();
            System.out.println("options include all supported FlameGraph options, plus the following:");
            System.out.println("  --cpu        CPU Flame Graph");
            System.out.println("  --alloc      Allocation Flame Graph");
            System.out.println("  --lock       Lock contention Flame Graph");
            System.out.println("  --threads    Split profile by threads");
            System.out.println("  --total      Accumulate the total value (time, bytes, etc.)");
            System.out.println("  --lines      Show line numbers");
            System.out.println("  --bci        Show bytecode indices");
            System.out.println("  --simple     Simple class names instead of FQN");
            System.out.println("  --dot        Dotted class names");
            System.out.println("  --from TIME  Start time in ms (absolute or relative)");
            System.out.println("  --to TIME    End time in ms (absolute or relative)");
            System.out.println("  --collapsed  Use collapsed stacks output format");
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
