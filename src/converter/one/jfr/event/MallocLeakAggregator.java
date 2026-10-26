/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Set;

public class MallocLeakAggregator implements EventCollector {
    private final EventCollector wrapped;
    private final double tail;
    private final Map<Long, IndexedMallocEvent> addresses;
    private List<MallocEvent> events;
    private Set<Long> survivingEvents;
    private long eventIndex;
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
        if (survivingEvents == null) {
            MallocEvent malloc = (MallocEvent) e;
            events.add(malloc.size > 0 ? new IndexedMallocEvent(malloc, eventIndex) : malloc);
            minTime = Math.min(minTime, e.time);
            maxTime = Math.max(maxTime, e.time);
        } else if (survivingEvents.remove(eventIndex)) {
            wrapped.collect(e);
        }
        eventIndex++;
    }

    public void skipEvent() {
        eventIndex++;
    }

    @Override
    public void beforeChunk() {
        if (survivingEvents == null) {
            events = new ArrayList<>();
        } else {
            wrapped.beforeChunk();
        }
    }

    @Override
    public void afterChunk() {
        if (survivingEvents != null) {
            wrapped.afterChunk();
            return;
        }

        events.sort(null);

        for (MallocEvent e : events) {
            if (e.size > 0) {
                addresses.put(e.address, (IndexedMallocEvent) e);
            } else {
                addresses.remove(e.address);
            }
        }

        events = null;
    }

    // Replay the event stream, forwarding only surviving allocations.
    // Event ordinals distinguish reused addresses and chunk-local stack/thread IDs.
    public void startReplay() {
        // Ignore tail allocations made in the last N% of profiling session:
        // they are too young to be considered a leak
        long timeCutoff = (long) (minTime * tail + maxTime * (1.0 - tail));

        survivingEvents = new HashSet<>();
        for (IndexedMallocEvent e : addresses.values()) {
            if (e.time <= timeCutoff) {
                survivingEvents.add(e.index);
            }
        }
        addresses.clear();
        eventIndex = 0;
    }

    @Override
    public boolean finish() {
        survivingEvents.clear();
        return wrapped.finish();
    }

    @Override
    public void forEach(Visitor visitor) {
        wrapped.forEach(visitor);
    }

    private static class IndexedMallocEvent extends MallocEvent {
        final long index;

        IndexedMallocEvent(MallocEvent event, long index) {
            super(event.time, event.tid, event.stackTraceId, event.address, event.size);
            this.index = index;
        }
    }
}
