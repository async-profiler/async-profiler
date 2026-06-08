/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import one.jfr.Dictionary;

import java.util.Map;
import java.util.TreeMap;

public final class Filter {
    // Per-thread timeline of open intervals, keyed by thread id
    private final Dictionary<Intervals> intervals;

    public Filter(Map<Integer, TreeMap<Long, Integer>> deltas) {
        this.intervals = new Dictionary<>(deltas.size());
        deltas.forEach((tid, threadDeltas) -> intervals.put(tid, new Intervals(threadDeltas)));
    }

    public boolean matches(int tid, long time) {
        Intervals ins = intervals.get(tid);
        return ins != null && ins.enclosing(time) > 0;
    }

    // Sort all starts and ends and count the number of open intervals (depth) at each boundary
    private static final class Intervals {
        private final long[] bounds;
        private final int[] depth;

        Intervals(TreeMap<Long, Integer> deltas) {
            long[] bounds = new long[deltas.size()];
            int[] depth = new int[deltas.size()];

            int i = 0;
            int open = 0;
            for (Map.Entry<Long, Integer> entry : deltas.entrySet()) {
                bounds[i] = entry.getKey();
                depth[i] = open += entry.getValue();
                i++;
            }

            this.bounds = bounds;
            this.depth = depth;
        }

        // Returns how many intervals enclose the given point
        int enclosing(long point) {
            int lo = 0;
            int hi = bounds.length;
            while (lo < hi) {
                int mid = (lo + hi) >>> 1;
                if (bounds[mid] <= point) {
                    lo = mid + 1;
                } else {
                    hi = mid;
                }
            }
            return lo > 0 ? depth[lo - 1] : 0;
        }
    }
}
