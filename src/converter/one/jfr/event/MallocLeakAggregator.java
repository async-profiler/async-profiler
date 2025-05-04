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
        MallocEvent me = (MallocEvent) e;
        events.add(me);
        minTime = Math.min(minTime, me.time);
        maxTime = Math.max(maxTime, me.time);
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
        // Ignore allocations made in the last 10% of profiling session:
        // they are too young to be considered a leak
        long tail = (long) (minTime * 0.1 + maxTime * 0.9);

        wrapped.beforeChunk();
        for (Event e : addresses.values()) {
            if (e.time < tail) {
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
