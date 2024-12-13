/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.List;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Map;
import java.util.HashMap;

public class MallocLeakAggregator implements IEventAggregator {
    private final IEventAggregator wrapped;
    private List<Event> events;
    private double grain = 0;

    Map<Long, MallocEvent> addresses = new HashMap<>();

    public MallocLeakAggregator(IEventAggregator wrapped) {
        this.wrapped = wrapped;
        this.events = new ArrayList<Event>();
    }

    private List<Event> filter(List<Event> events) {
        for (Event event : events) {
            if (!(event instanceof MallocEvent)) {
                continue;
            }

            MallocEvent e = (MallocEvent) event;
            if (e.size > 0) {
                addresses.put(e.address, e);
            } else {
                addresses.remove(e.address);
            }
        }

        return new ArrayList<>(addresses.values());
    }

    public void collect(Event e) {
        events.add(e);
    }

    public void finishChunk() {
        Collections.sort(events);
        events = filter(events);
    }

    public void finish() {
        if (grain > 0) {
            wrapped.coarsen(grain);
        }

        for (Event e : events) {
            wrapped.collect(e);
        }
    }

    public void coarsen(double grain) {
        // Delay coarsening until the final chunk is processed.
        this.grain = grain;
    }

    public void resetChunk() {
        wrapped.resetChunk();
    }

    public void forEach(IEventAcceptor.Visitor visitor) {
        wrapped.forEach(visitor);
    }

    public void forEach(IEventAcceptor.ValueVisitor visitor) {
        wrapped.forEach(visitor);
    }
}
