/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import one.convert.TimeIntervals;
import one.jfr.JfrReader;

import java.util.BitSet;

// Delegating implementation of {@link EventCollector} which allows filtering events
// based on several criteria.
public final class FilteringCollector implements EventCollector {

    private final TimeIntervals timeIntervals;
    private final EventCollector delegate;
    private final long from;
    private final long to;
    private final BitSet threadStates;
    private final JfrReader jfr;

    // Needs to be reset at each chunk start
    private long fromTicks;
    private long toTicks;

    public FilteringCollector(EventCollector delegate, TimeIntervals timeIntervals, long from, long to,
                              BitSet threadStates, JfrReader jfr) {
        this.delegate = delegate;
        this.timeIntervals = timeIntervals;
        this.from = from;
        this.to = to;
        this.threadStates = threadStates;
        this.jfr = jfr;
    }

    @Override
    public void collect(Event event) {
        if (event.time >= fromTicks && event.time <= toTicks &&
                (threadStates == null || threadStates.get(((ExecutionSample) event).threadState)) &&
                (timeIntervals == null || timeIntervals.belongs(jfr.eventTimeToNanos(event.time)))) {
            delegate.collect(event);
        }
    }

    @Override
    public void beforeChunk() {
        fromTicks = from != 0 ? toTicks(from) : Long.MIN_VALUE;
        toTicks = to != 0 ? toTicks(to) : Long.MAX_VALUE;
        delegate.beforeChunk();
    }

    // millis can be an absolute timestamp or an offset from the beginning/end of the recording
    private long toTicks(long millis) {
        long nanos = millis * 1_000_000;
        if (millis < 0) {
            nanos += jfr.endNanos;
        } else if (millis < 1500000000000L) {
            nanos += jfr.startNanos;
        }
        return (long) ((nanos - jfr.chunkStartNanos) * (jfr.ticksPerSec / 1e9)) + jfr.chunkStartTicks;
    }

    @Override
    public void afterChunk() {
        delegate.afterChunk();
    }

    @Override
    public boolean finish() {
        return delegate.finish();
    }

    @Override
    public void forEach(Visitor visitor) {
        delegate.forEach(visitor);
    }
}
