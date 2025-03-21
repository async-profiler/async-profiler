/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import one.jfr.JfrReader;

import java.io.IOException;
import java.io.UncheckedIOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class SpanFilter implements EventCollector {
    private final Map<Integer, List<SpanEvent>> spans = new HashMap<>();
    private final JfrReader jfr;
    private final EventCollector wrapped;
    private final SpanFilterCriteria spanFilterCriteria;

    public SpanFilter(JfrReader jfr, EventCollector wrapped, String filter) {
        this.jfr = jfr;
        this.wrapped = wrapped;
        this.spanFilterCriteria = new SpanFilterCriteria(filter);
    }

    private boolean shouldCollect(int tid, long time) {
        List<SpanEvent> threadSpans = spans.get(tid);
        if (threadSpans == null || threadSpans.isEmpty()) {
            return false;
        }

        int index = Collections.binarySearch(
                threadSpans,
                null,
                (span1, span2) -> {
                    if (time >= span1.time && time <= span1.time + span1.duration) {
                        return 0;
                    }
                    return Long.compare(span1.time, time);
                }
        );

        return index >= 0;
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
                if (spanFilterCriteria.matches(event)) {
                    spans.computeIfAbsent(event.tid, tid -> new ArrayList<>()).add(event);
                }
            }
            spans.forEach((tid, spanList) -> Collections.sort(spanList));

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
