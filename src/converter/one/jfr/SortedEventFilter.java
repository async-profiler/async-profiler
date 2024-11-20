/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

import one.jfr.event.Event;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

public abstract class SortedEventFilter implements IEventReader {
    private List<Event> events;
    private final IEventReader raw;

    private boolean initialized;
    private int cursor;

    SortedEventFilter(IEventReader raw) {
        this.raw = raw;
        events = new ArrayList<>();
        initialized = false;
        cursor = 0;
    }

    @SuppressWarnings("unchecked")
    private <E extends Event> void initialize(Class<E> cls) throws IOException {
        events = (List<Event>) filter(raw.readAllEvents(cls));
        initialized = true;
    }

    protected abstract <E extends Event> List<E> filter(List<E> events);

    @Override
    @SuppressWarnings("unchecked")
    public <E extends Event> List<E> readAllEvents(Class<E> cls) throws IOException {
        if (!initialized) {
            initialize(cls);
        }

        return (List<E>) events;
    }

    @Override
    @SuppressWarnings("unchecked")
    public <E extends Event> E readEvent(Class<E> cls) throws IOException {
        if (!initialized) {
            initialize(cls);
        }

        if (cursor < events.size()) {
            return (E) events.get(cursor++);
        }

        return null;
    }
}
