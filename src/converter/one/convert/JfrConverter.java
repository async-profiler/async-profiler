/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import one.jfr.ClassRef;
import one.jfr.Dictionary;
import one.jfr.JfrReader;
import one.jfr.MethodRef;
import one.jfr.event.*;

import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.util.BitSet;
import java.util.Map;
import java.util.function.Consumer;

import static one.convert.Frame.*;

public abstract class JfrConverter extends Classifier {
    protected final JfrReader jfr;
    protected final Arguments args;
    protected final EventCollector collector;
    protected Dictionary<String> methodNames;

    public JfrConverter(JfrReader jfr, Arguments args) {
        this.jfr = jfr;
        this.args = args;

        EventCollector collector = createCollector(args);
        this.collector = args.nativemem && args.leak ? new MallocLeakAggregator(collector, args.tail) : collector;
    }

    public void convert() throws IOException {
        TimeIntervals timeIntervals = args.latency > -1 ? readLatencyTimeIntervals() : null;
        EventCollector filteringCollector = new FilteringCollector(collector, timeIntervals, args.from, args.to,
                makeThreadStatesBitSet(), jfr);

        jfr.stopAtNewChunk = true;
        while (jfr.hasMoreChunks()) {
            // Reset method dictionary, since new chunk may have different IDs
            methodNames = new Dictionary<>();

            filteringCollector.beforeChunk();
            collectEvents(filteringCollector::collect);
            filteringCollector.afterChunk();

            convertChunk();
        }

        if (filteringCollector.finish()) {
            convertChunk();
        }
    }

    private TimeIntervals readLatencyTimeIntervals() throws IOException {
        jfr.stopAtNewChunk = true;
        TimeIntervals intervals = new TimeIntervals();
        while (jfr.hasMoreChunks()) {
            long minLatencyTicks = args.latency * jfr.ticksPerSec / 1000;
            collectEvents(MethodTrace.class, methodTrace -> {
                if (methodTrace.duration >= minLatencyTicks) {
                    intervals.add(jfr.eventTimeToNanos(methodTrace.time), jfr.eventTimeToNanos(methodTrace.time + methodTrace.duration));
                }
            });
        }
        jfr.reset();
        return intervals;
    }

    protected EventCollector createCollector(Arguments args) {
        return new EventAggregator(args.threads, args.grain);
    }

    protected void collectEvents(Consumer<Event> eventConsumer) throws IOException {
        Class<? extends Event> eventClass = args.nativelock ? NativeLockEvent.class
                : args.nativemem ? MallocEvent.class
                : args.live ? LiveObject.class
                : args.alloc ? AllocationSample.class
                : args.lock ? ContendedLock.class
                : args.trace ? MethodTrace.class
                : ExecutionSample.class;

        collectEvents(eventClass, eventConsumer::accept);
    }

    private <E extends Event> void collectEvents(Class<E> eventClass, Consumer<E> eventConsumer) throws IOException {
        for (E event; (event = jfr.readEvent(eventClass)) != null; ) {
            eventConsumer.accept(event);
        }
    }

    private BitSet makeThreadStatesBitSet() {
        BitSet threadStates = null;
        if (args.state != null) {
            threadStates = new BitSet();
            for (String state : args.state.toUpperCase().split(",")) {
                threadStates.set(toThreadState(state));
            }
        } else if (args.cpu) {
            threadStates = getThreadStates(true);
        } else if (args.wall) {
            threadStates = getThreadStates(false);
        } else if (args.cpuTime) {
            threadStates = new BitSet();
            threadStates.set(ExecutionSample.CPU_TIME_SAMPLE);
        }
        return threadStates;
    }

    protected void convertChunk() {
        // To be overridden in subclasses
    }

    protected int toThreadState(String name) {
        Map<Integer, String> threadStates = jfr.enums.get("jdk.types.ThreadState");
        if (threadStates != null) {
            for (Map.Entry<Integer, String> entry : threadStates.entrySet()) {
                if (entry.getValue().startsWith(name, 6)) {
                    return entry.getKey();
                }
            }
        }
        throw new IllegalArgumentException("Unknown thread state: " + name);
    }

    protected BitSet getThreadStates(boolean cpu) {
        BitSet set = new BitSet();
        Map<Integer, String> threadStates = jfr.enums.get("jdk.types.ThreadState");
        if (threadStates != null) {
            for (Map.Entry<Integer, String> entry : threadStates.entrySet()) {
                set.set(entry.getKey(), "STATE_DEFAULT".equals(entry.getValue()) == cpu);
            }
        }
        return set;
    }

    @Override
    public String getMethodName(long methodId, byte methodType) {
        String result = methodNames.get(methodId);
        if (result == null) {
            methodNames.put(methodId, result = resolveMethodName(methodId, methodType));
        }
        return result;
    }

    private String resolveMethodName(long methodId, byte methodType) {
        MethodRef method = jfr.methods.get(methodId);
        if (method == null) {
            return "unknown";
        }

        ClassRef cls = jfr.classes.get(method.cls);
        byte[] className = jfr.symbols.get(cls.name);
        byte[] methodName = jfr.symbols.get(method.name);

        if (className == null || className.length == 0 || isNativeFrame(methodType)) {
            return new String(methodName, StandardCharsets.UTF_8);
        } else {
            String classStr = toJavaClassName(className, 0, args.dot);
            if (methodName == null || methodName.length == 0) {
                return classStr;
            }
            String methodStr = new String(methodName, StandardCharsets.UTF_8);
            return classStr + '.' + methodStr;
        }
    }

    public String getClassName(long classId) {
        ClassRef cls = jfr.classes.get(classId);
        if (cls == null) {
            return "null";
        }
        byte[] className = jfr.symbols.get(cls.name);

        int arrayDepth = 0;
        while (className[arrayDepth] == '[') {
            arrayDepth++;
        }

        String name = toJavaClassName(className, arrayDepth, true);
        while (arrayDepth-- > 0) {
            name = name.concat("[]");
        }
        return name;
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

        if (args.norm) {
            for (int i = end - 2; i > start; i--) {
                if (symbol[i] == '/' || symbol[i] == '.') {
                    if (symbol[i + 1] >= '0' && symbol[i + 1] <= '9') {
                        end = i;
                        if (i > start + 19 && symbol[i - 19] == '+' && symbol[i - 18] == '0') {
                            // Original JFR transforms lambda names to something like
                            // pkg.ClassName$$Lambda+0x00007f8177090218/543846639
                            end = i - 19;
                        }
                    }
                    break;
                }
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

    public StackTraceElement getStackTraceElement(long methodId, byte methodType, int location) {
        MethodRef method = jfr.methods.get(methodId);
        if (method == null) {
            return new StackTraceElement("", "unknown", null, 0);
        }

        ClassRef cls = jfr.classes.get(method.cls);
        byte[] className = jfr.symbols.get(cls.name);
        byte[] methodName = jfr.symbols.get(method.name);

        String classStr = className == null || className.length == 0 || isNativeFrame(methodType) ? "" :
                toJavaClassName(className, 0, args.dot);
        String methodStr = methodName == null || methodName.length == 0 ? "" :
                new String(methodName, StandardCharsets.UTF_8);
        return new StackTraceElement(classStr, methodStr, null, location >>> 16);
    }

    public String getThreadName(int tid) {
        String threadName = jfr.threads.get(tid);
        return threadName == null ? "[tid=" + tid + ']' :
                threadName.startsWith("[tid=") ? threadName : '[' + threadName + " tid=" + tid + ']';
    }

    protected boolean isNativeFrame(byte methodType) {
        // In JDK Flight Recorder, TYPE_NATIVE denotes Java native methods,
        // while in async-profiler, TYPE_NATIVE is for C methods
        return methodType == TYPE_NATIVE && jfr.getEnumValue("jdk.types.FrameType", TYPE_KERNEL) != null ||
                methodType == TYPE_CPP ||
                methodType == TYPE_KERNEL;
    }

    public String getValueType() {
        if (args.nativemem) return "malloc";
        if (args.alloc || args.live) return "allocations";
        if (args.lock) return "locks";
        return "cpu";
    }

    public String getSampleUnits() {
        return "count";
    }

    public String getTotalUnits() {
        if (args.nativemem || args.alloc || args.live) return "bytes";
        return "nanoseconds";
    }

    public double counterFactor() {
        return (args.lock || args.nativelock) ? 1e9 / jfr.ticksPerSec : 1.0;
    }

    // Select sum(samples) or sum(value) depending on the --total option.
    // For lock and nativelock events, convert lock duration from ticks to nanoseconds.
    protected abstract class AggregatedEventVisitor implements EventCollector.Visitor {
        private final double factor = !args.total ? 0.0 : counterFactor();

        @Override
        public final void visit(Event event, long samples, long value) {
            visit(event, factor == 0.0 ? samples : factor == 1.0 ? value : (long) (value * factor));
        }

        protected abstract void visit(Event event, long value);
    }
}
