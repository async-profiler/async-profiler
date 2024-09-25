/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class EventAggregator {
    private static final int INITIAL_CAPACITY = 1024;

    private final boolean threads;
    private final boolean total;
    private Event[] keys;
    private long[] values;
    private int size;

    public EventAggregator(boolean threads, boolean total) {
        this.threads = threads;
        this.total = total;
        this.keys = new Event[INITIAL_CAPACITY];
        this.values = new long[INITIAL_CAPACITY];
    }

    public void collect(Event e) {
        int mask = keys.length - 1;
        int i = hashCode(e) & mask;
        while (keys[i] != null) {
            if (sameGroup(keys[i], e)) {
                values[i] += total ? e.value() : e.samples();
                return;
            }
            i = (i + 1) & mask;
        }

        keys[i] = e;
        values[i] = total ? e.value() : e.samples();

        if (++size * 2 > keys.length) {
            resize(keys.length * 2);
        }
    }

    public long getValue(Event e) {
        int mask = keys.length - 1;
        int i = hashCode(e) & mask;
        while (keys[i] != null && !sameGroup(keys[i], e)) {
            i = (i + 1) & mask;
        }
        return values[i];
    }

    public void forEach(Visitor visitor) {
        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                visitor.visit(keys[i], values[i]);
            }
        }
    }

    private int hashCode(Event e) {
        return e.hashCode() + (threads ? e.tid * 31 : 0);
    }

    private boolean sameGroup(Event e1, Event e2) {
        return e1.stackTraceId == e2.stackTraceId && (!threads || e1.tid == e2.tid) && e1.sameGroup(e2);
    }

    private void resize(int newCapacity) {
        Event[] newKeys = new Event[newCapacity];
        long[] newValues = new long[newCapacity];
        int mask = newKeys.length - 1;

        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                for (int j = hashCode(keys[i]) & mask; ; j = (j + 1) & mask) {
                    if (newKeys[j] == null) {
                        newKeys[j] = keys[i];
                        newValues[j] = values[i];
                        break;
                    }
                }
            }
        }

        keys = newKeys;
        values = newValues;
    }

    public interface Visitor {
        void visit(Event event, long value);
    }
}
