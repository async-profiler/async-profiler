/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class EventAggregator {
    private static final int INITIAL_CAPACITY = 1024;

    private final boolean threads;
    private final boolean total;
    private final double factor;
    private Event[] keys;
    private long[] samples;
    private long[] values;
    private int size;
    private double fraction;

    public EventAggregator(boolean threads, boolean total) {
        this(threads, total, 1.0);
    }

    public EventAggregator(boolean threads, boolean total, double factor) {
        this.threads = threads;
        this.total = total;
        this.factor = factor;
        this.keys = new Event[INITIAL_CAPACITY];
        this.samples = new long[INITIAL_CAPACITY];
        this.values = new long[INITIAL_CAPACITY];
    }

    public int size() {
        return size;
    }

    public void collect(Event e) {
        collect(e, e.samples(), e.value());
    }

    public void collect(Event e, long samples, long value) {
        int mask = keys.length - 1;
        int i = hashCode(e) & mask;
        while (keys[i] != null) {
            if (sameGroup(keys[i], e)) {
                this.samples[i] += samples;
                this.values[i] += value;
                return;
            }
            i = (i + 1) & mask;
        }

        this.keys[i] = e;
        this.samples[i] = samples;
        this.values[i] = value;

        if (++size * 2 > keys.length) {
            resize(keys.length * 2);
        }
    }

    public void forEach(Visitor visitor) {
        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                visitor.visit(keys[i], samples[i], values[i]);
            }
        }
    }

    public void forEach(ValueVisitor visitor) {
        double factor = total ? this.factor : 0.0;
        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                visitor.visit(keys[i], factor == 0.0 ? samples[i] : factor == 1.0 ? values[i] : (long) (values[i] * factor));
            }
        }
    }

    public void coarsen(double grain) {
        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                long s0 = samples[i];
                long s1 = round(s0 / grain);
                if (s1 == 0) {
                    keys[i] = null;
                    size--;
                }
                samples[i] = s1;
                values[i] = (long) (values[i] * ((double) s1 / s0));
            }
        }
    }

    private long round(double d) {
        long r = (long) d;
        if ((fraction += d - r) >= 1.0) {
            fraction -= 1.0;
            r++;
        }
        return r;
    }

    private int hashCode(Event e) {
        return e.hashCode() + (threads ? e.tid * 31 : 0);
    }

    private boolean sameGroup(Event e1, Event e2) {
        return e1.stackTraceId == e2.stackTraceId && (!threads || e1.tid == e2.tid) && e1.sameGroup(e2);
    }

    private void resize(int newCapacity) {
        Event[] newKeys = new Event[newCapacity];
        long[] newSamples = new long[newCapacity];
        long[] newValues = new long[newCapacity];
        int mask = newKeys.length - 1;

        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                for (int j = hashCode(keys[i]) & mask; ; j = (j + 1) & mask) {
                    if (newKeys[j] == null) {
                        newKeys[j] = keys[i];
                        newSamples[j] = samples[i];
                        newValues[j] = values[i];
                        break;
                    }
                }
            }
        }

        keys = newKeys;
        samples = newSamples;
        values = newValues;
    }

    public interface Visitor {
        void visit(Event event, long samples, long value);
    }

    public interface ValueVisitor {
        void visit(Event event, long value);
    }
}
