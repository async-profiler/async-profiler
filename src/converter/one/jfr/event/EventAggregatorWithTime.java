/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.Arrays;

public class EventAggregatorWithTime implements EventCollector {
    private static final int INITIAL_CAPACITY = 1024;

    private final boolean threads;
    private final boolean total;
    private Event[] keys;
    // samples or values
    private long[][] contents;
    private long[][] timestamps;
    private int[] timestampCount;
    private int size;

    public EventAggregatorWithTime(boolean threads, boolean total) {
        this.threads = threads;
        this.total = total;

        beforeChunk();
    }

    @Override
    public void collect(Event e) {
        collect(e, e.samples(), e.value());
    }

    private void doRecordTimestamp(int i, long timestamp, long content) {
        if (timestamps[i] == null) {
            timestamps[i] = new long[1];
            contents[i] = new long[1];
        } else if (timestamps[i].length <= timestampCount[i]) {
            int newSize = timestamps[i].length * 2;
            timestamps[i] = Arrays.copyOf(timestamps[i], newSize);
            contents[i] = Arrays.copyOf(contents[i], newSize);
        }
        timestamps[i][timestampCount[i]] = timestamp;
        contents[i][timestampCount[i]] += content;
        timestampCount[i] += 1;
    }

    public void collect(Event e, long samples, long value) {
        int mask = keys.length - 1;
        int i = hashCode(e) & mask;
        while (keys[i] != null) {
            if (sameGroup(keys[i], e)) {
                doRecordTimestamp(i, e.time, this.total ? value : samples);
                return;
            }
            i = (i + 1) & mask;
        }

        this.keys[i] = e;
        doRecordTimestamp(i, e.time, this.total ? value : samples);

        if (++size * 2 > keys.length) {
            resize(keys.length * 2);
        }
    }

    @Override
    public void beforeChunk() {
        if (keys == null || size > 0) {
            keys = new Event[INITIAL_CAPACITY];
            contents = new long[INITIAL_CAPACITY][];
            timestamps = new long[INITIAL_CAPACITY][];
            timestampCount = new int[INITIAL_CAPACITY];
            size = 0;
        }
    }

    @Override
    public void afterChunk() {}

    @Override
    public boolean finish() {
        keys = null;
        contents = null;
        timestamps = null;
        timestampCount = null;
        return false;
    }

    @Override
    public void forEach(EventCollector.Visitor visitor) {
        throw new UnsupportedOperationException("Should not be called");
    }

    public void forEach(Visitor visitor) {
        if (size > 0) {
            for (int i = 0; i < keys.length; i++) {
                if (keys[i] != null) {
                    visitor.visit(keys[i], Arrays.copyOf(contents[i], timestampCount[i]), Arrays.copyOf(timestamps[i], timestampCount[i]));
                }
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
        long[][] newContents = new long[newCapacity][];
        long[][] newTimestamps = new long[newCapacity][];
        int[] newTimestampsCount = new int[newCapacity];

        int mask = newKeys.length - 1;

        for (int i = 0; i < keys.length; i++) {
            if (keys[i] != null) {
                for (int j = hashCode(keys[i]) & mask; ; j = (j + 1) & mask) {
                    if (newKeys[j] == null) {
                        newKeys[j] = keys[i];
                        newContents[j] = contents[i];
                        newTimestamps[j] = timestamps[i];
                        newTimestampsCount[j] = timestampCount[i];
                        break;
                    }
                }
            }
        }

        keys = newKeys;
        contents = newContents;
        timestamps = newTimestamps;
        timestampCount = newTimestampsCount;
    }

    public static interface Visitor {
        void visit(Event event, long[] contents, long[] timestamps);
    }
}
