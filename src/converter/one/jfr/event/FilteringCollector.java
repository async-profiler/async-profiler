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

    private final EventCollector delegate;
    private final long fromMillis;
    private final long toMillis;
    private final BitSet threadStates;
    private final JfrReader jfr;

    private TimeIntervals timeIntervals;

    // Needs to be reset at each chunk start
    private long fromTicks;
    private long toTicks;

    public FilteringCollector(EventCollector delegate, long fromMillis, long toMillis, BitSet threadStates, JfrReader jfr) {
        this.delegate = delegate;
        this.fromMillis = fromMillis;
        this.toMillis = toMillis;
        this.threadStates = threadStates;
        this.jfr = jfr;
    }

    public void setTimeIntervals(TimeIntervals timeIntervals) {
        this.timeIntervals = timeIntervals;
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
        fromTicks = fromMillis != 0 ? toTicks(fromMillis) : Long.MIN_VALUE;
        toTicks = toMillis != 0 ? toTicks(toMillis) : Long.MAX_VALUE;
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
