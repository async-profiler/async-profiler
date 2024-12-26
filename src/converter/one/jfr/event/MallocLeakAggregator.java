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
    private final EventAggregator wrapped;
    private final Map<Long, MallocEvent> addresses;
    private List<MallocEvent> events;

    public MallocLeakAggregator(EventAggregator wrapped) {
        this.wrapped = wrapped;
        this.addresses = new HashMap<>();
        this.events = new ArrayList<>();
    }

    @Override
    public void collect(Event e) {
        events.add((MallocEvent) e);
    }

    @Override
    public void finishChunk() {
        events.sort(null);

        for (MallocEvent e : events) {
            if (e.size > 0) {
                addresses.put(e.address, e);
            } else {
                addresses.remove(e.address);
            }
        }
    }

    @Override
    public void resetChunk() {
        events = new ArrayList<>();
    }

    @Override
    public boolean finish() {
        for (Event e : addresses.values()) {
            wrapped.collect(e);
        }
        wrapped.finishChunk();

        // Free memory before the final conversion
        addresses.clear();
        return true;
    }

    @Override
    public void forEach(Visitor visitor) {
        wrapped.forEach(visitor);
    }
}
