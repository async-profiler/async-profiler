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
    // Ignore allocations made in the last 10% of profiling session:
    // they are too young to be considered a leak
    private static final double TAIL = 0.1;

    private final EventCollector wrapped;
    private final Map<Long, MallocEvent> addresses;
    private List<MallocEvent> events;
    private long minTime = Long.MAX_VALUE;
    private long maxTime = Long.MIN_VALUE;

    public MallocLeakAggregator(EventCollector wrapped) {
        this.wrapped = wrapped;
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
        // Do not consider as leaks allocations made after the cutoff
        long timeCutoff = (long) (minTime * TAIL + maxTime * (1.0 - TAIL));

        wrapped.beforeChunk();
        for (Event e : addresses.values()) {
            if (e.time < timeCutoff) {
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
