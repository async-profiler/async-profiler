/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import one.jfr.JfrReader;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class SpanFilter implements EventCollector {
    private final Map<Integer, List<SpanEvent>> spans = new HashMap<>();
    private final JfrReader jfr;
    private final EventCollector wrapped;

    public SpanFilter(JfrReader jfr, EventCollector wrapped, String filter) {
        this.jfr = jfr;
        this.wrapped = wrapped;

        // TODO: parse filter
    }

    private boolean matchesFilter(String tag, long duration) {
        // TODO: check if span matches filter
        return false;
    }

    private boolean shouldCollect(int tid, long time) {
        // TODO: check if tid/time falls into any of spans
        return false;
    }

    @Override
    public void collect(Event e) {
        if (shouldCollect(e.tid, e.time)) {
            wrapped.collect(e);
        }
    }

    @Override
    public void beforeChunk() {
        try {
            for (SpanEvent event; (event = jfr.readEvent(SpanEvent.class)) != null; ) {
                if (matchesFilter(event.tag, event.duration)) {
                    spans.computeIfAbsent(event.tid, tid -> new ArrayList<>()).add(event);
                }
            }
            jfr.rewindChunk();
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    @Override
    public void afterChunk() {
        spans.clear();
    }

    @Override
    public boolean finish() {
        return false;
    }

    @Override
    public void forEach(Visitor visitor) {
        wrapped.forEach(visitor);
    }
}
