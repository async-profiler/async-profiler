/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package one.jfr.event;

import java.util.*;

/** Replays observed mapping ranges; retains original allocation identity after splitting. */
public class MappingLeakAggregator implements EventCollector {
    private static final class Range {
        final long start, end;
        final MappingEvent allocation;
        Range(long start, long end, MappingEvent allocation) {
            this.start = start; this.end = end; this.allocation = allocation;
        }
    }
    private final TreeMap<Long, Range> ranges = new TreeMap<>(Long::compareUnsigned);
    private final EventCollector wrapped;
    private final double tail;
    private final List<MappingEvent> events = new ArrayList<>();
    private long first = Long.MAX_VALUE, last = Long.MIN_VALUE;
    public MappingLeakAggregator(EventCollector wrapped, double tail) {
        if (!(tail >= 0 && tail <= 1)) throw new IllegalArgumentException("tail must be between 0 and 1");
        this.wrapped = wrapped; this.tail = tail;
    }
    public void beforeChunk() { events.clear(); }
    public void collect(Event event) { events.add((MappingEvent)event); }
    public void afterChunk() {
        events.sort(null);
        for (MappingEvent e : events) {
            if (e.size <= 0 || Long.compareUnsigned(e.address + e.size, e.address) <= 0)
                throw new IllegalArgumentException("Invalid mapping range");
            first = Math.min(first, e.time); last = Math.max(last, e.time);
            long end = e.address + e.size;
            subtract(e.address, end);
            if (!e.unmap) ranges.put(e.address, new Range(e.address, end, e));
        }
        events.clear();
    }
    private void subtract(long start, long end) {
        Map.Entry<Long, Range> entry = ranges.floorEntry(start);
        if (entry == null || Long.compareUnsigned(entry.getValue().end, start) <= 0)
            entry = ranges.ceilingEntry(start);
        while (entry != null && Long.compareUnsigned(entry.getKey(), end) < 0) {
            Range r = entry.getValue();
            ranges.remove(r.start);
            if (Long.compareUnsigned(r.start, start) < 0)
                ranges.put(r.start, new Range(r.start, start, r.allocation));
            if (Long.compareUnsigned(r.end, end) > 0) {
                ranges.put(end, new Range(end, r.end, r.allocation));
                break;
            }
            entry = ranges.ceilingEntry(start);
        }
    }
    public boolean finish() {
        IdentityHashMap<MappingEvent, Long> remaining = new IdentityHashMap<>();
        for (Range r : ranges.values()) remaining.merge(r.allocation, r.end - r.start, Long::sum);
        long cutoff = tail == 0 ? last : (long)(first * tail + last * (1 - tail));
        wrapped.beforeChunk();
        for (Map.Entry<MappingEvent, Long> r : remaining.entrySet()) {
            MappingEvent e = r.getKey();
            if (e.time <= cutoff) wrapped.collect(new MappingEvent(e.time, e.tid, e.stackTraceId, e.address, r.getValue(), false));
        }
        wrapped.afterChunk();
        ranges.clear();
        return true;
    }
    public void forEach(Visitor visitor) { wrapped.forEach(visitor); }
}
