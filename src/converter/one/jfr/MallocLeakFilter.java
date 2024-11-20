/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr;

import one.jfr.event.Event;
import one.jfr.event.MallocEvent;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class MallocLeakFilter extends SortedEventFilter {

    public MallocLeakFilter(IEventReader raw) {
        super(raw);
    }

    @Override
    @SuppressWarnings("unchecked")
    protected <E extends Event> List<E> filter(List<E> events) {
        Map<Long, MallocEvent> addresses = new HashMap<>();

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

        return (List<E>) new ArrayList<>(addresses.values());
    }
}
