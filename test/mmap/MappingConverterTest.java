/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
import one.jfr.event.*;
import java.util.*;

public class MappingConverterTest {
    static MappingEvent map(long time, long addr, long size) { return new MappingEvent(time, 1, (int)time, addr, size, false); }
    static MappingEvent unmap(long time, long addr, long size) { return new MappingEvent(time, 2, 0, addr, size, true); }
    static long[] replay(double tail, MappingEvent[]... chunks) {
        MappingLeakAggregator a = new MappingLeakAggregator(new EventAggregator(false, 0), tail);
        for (MappingEvent[] chunk : chunks) {
            a.beforeChunk(); for (MappingEvent e : chunk) a.collect(e); a.afterChunk();
        }
        a.finish();
        long[] totals = new long[2];
        a.forEach((e, samples, value) -> { totals[0] += samples; totals[1] += value; });
        return totals;
    }
    static void check(long count, long bytes, MappingEvent... events) {
        long[] got = replay(0, events);
        if (got[0] != count || got[1] != bytes) throw new AssertionError(Arrays.toString(got));
    }
    public static void main(String[] args) {
        check(0, 0, map(1, 0x1000, 0x3000), unmap(2, 0x1000, 0x3000));
        check(1, 0x2000, map(1, 0x1000, 0x3000), unmap(2, 0x2000, 0x1000));
        check(1, 0x2000, map(1, 0x1000, 0x3000), unmap(2, 0x1000, 0x1000));
        check(1, 0x2000, map(1, 0x1000, 0x3000), unmap(2, 0x3000, 0x1000));
        check(0, 0, map(1, 0x1000, 0x1000), map(2, 0x3000, 0x1000), unmap(3, 0, 0x5000));
        check(2, 0x3000, map(1, 0x1000, 0x3000), map(2, 0x2000, 0x1000)); // MAP_FIXED
        check(1, 0x1000, map(1, 0x1000, 0x1000), unmap(2, 0x1000, 0x1000), map(3, 0x1000, 0x1000));
        check(1, 0x1000, map(1, Long.MIN_VALUE, 0x2000), unmap(2, Long.MIN_VALUE + 0x1000, 0x1000));
        long[] chunks = replay(0, new MappingEvent[]{map(1, 0x1000, 0x3000)}, new MappingEvent[]{unmap(2, 0x2000, 0x1000)});
        if (chunks[0] != 1 || chunks[1] != 0x2000) throw new AssertionError("chunk boundary");
        long[] tail = replay(0.1, new MappingEvent[]{map(1, 0x1000, 0x1000), map(100, 0x3000, 0x1000)});
        if (tail[0] != 1 || tail[1] != 0x1000) throw new AssertionError("tail");
        System.out.println("PASS: 10 mapping range/count/tail/chunk cases");
    }
}
