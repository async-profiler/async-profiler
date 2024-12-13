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

public class MallocLeakAggregator extends EventAggregator {
    private List<Event> events;

    Map<Long, MallocEvent> addresses = new HashMap<>();

    public MallocLeakAggregator(boolean threads, boolean total, double factor) {
        super(threads, total, factor);
        events = new ArrayList<Event>();
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
        System.out.println("events count: " + events.size());
        events = filter(events);
        System.out.println("events count 2: " + events.size());

    }

    public void finish() {
        for (Event e : events) {
            super.collect(e);
        }
    }
}
