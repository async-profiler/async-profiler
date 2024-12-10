/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import java.util.List;
import java.util.ArrayList;
import java.util.Collections;

public abstract class SortedEventAggregator extends EventAggregator {
    private List<Event> events;

    public SortedEventAggregator(boolean threads, boolean total) {
        super(threads, total);
        events = new ArrayList<Event>();
    }

    public SortedEventAggregator(boolean threads, boolean total, double factor) {
        super(threads, total, factor);
        events = new ArrayList<Event>();
    }

    protected abstract List<Event> filter(List<Event> events);

    public void collect(Event e) {
        events.add(e);
    }

    public void finishChunk() {
        Collections.sort(events);
        events = filter(events);
    }

    public void finish() {
        for (Event e : events) {
            super.collect(e);
        }
    }
}
