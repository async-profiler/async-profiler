/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import one.convert.Arguments;
import one.convert.TimeIntervals;

import java.util.BitSet;

// Delegating implementation of {@link EventCollector} which allows filtering events
// based on several criteria.
public final class FilteringCollector implements EventCollector {

    private final TimeIntervals timeIntervals;
    private final EventCollector delegate;
    private final long startTicks;
    private final long endTicks;
    private final BitSet threadStates;

    public FilteringCollector(EventCollector delegate, TimeIntervals timeIntervals, long startTicks, long endTicks,
                              BitSet threadStates) {
        this.delegate = delegate;
        this.timeIntervals = timeIntervals;
        this.startTicks = startTicks;
        this.endTicks = endTicks;
        this.threadStates = threadStates;
    }

    @Override
    public void collect(Event event) {
        if (event.time >= startTicks && event.time <= endTicks &&
                (threadStates == null || threadStates.get(((ExecutionSample) event).threadState)) &&
                (timeIntervals == null || timeIntervals.belongs(event.time))) {
            delegate.collect(event);
        }
    }

    @Override
    public void beforeChunk() {
        delegate.beforeChunk();
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
