/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;

public class MallocLeakAggregator implements EventCollector {
    private final EventCollector wrapped;
    private final double tail;
    private final Map<Long, MallocEvent> addresses;
    private List<MallocEvent> events;
    private long minTime = Long.MAX_VALUE;
    private long maxTime = Long.MIN_VALUE;

    public MallocLeakAggregator(EventCollector wrapped, double tail) {
        if (tail < 0.0 || tail > 1.0) {
            throw new IllegalArgumentException("tail must be between 0 and 1");
        }
        this.wrapped = wrapped;
        this.tail = tail;
        this.addresses = new HashMap<>();
    }

    @Override
    public void collect(Event e) {
        events.add((MallocEvent) e);
        minTime = Math.min(minTime, e.time);
        maxTime = Math.max(maxTime, e.time);
    }

    @Override
    public void beforeChunk() {
        events = new ArrayList<>();
    }

    @Override
    public void afterChunk() {
        events.sort(null);

        for (MallocEvent e : events) {
            if (e.size > 0) {
                addresses.put(e.address, e);
            } else {
                addresses.remove(e.address);
            }
        }

        events = null;
    }

    @Override
    public boolean finish() {
        // Ignore tail allocations made in the last N% of profiling session:
        // they are too young to be considered a leak
        long timeCutoff = (long) (minTime * tail + maxTime * (1.0 - tail));

        wrapped.beforeChunk();
        for (Event e : addresses.values()) {
            if (e.time <= timeCutoff) {
                wrapped.collect(e);
            }
        }
        wrapped.afterChunk();

        // Free memory before the final conversion
        addresses.clear();
        return true;
    }

    @Override
    public void forEach(Visitor visitor) {
        wrapped.forEach(visitor);
    }
}
