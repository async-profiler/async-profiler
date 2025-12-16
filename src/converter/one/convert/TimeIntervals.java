/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.util.Map;
import java.util.NavigableMap;
import java.util.TreeMap;

public final class TimeIntervals {
    // No overlapping intervals
    private final TreeMap<Long, Long> timeIntervals = new TreeMap<>();

    public void add(long startInstant, long endInstant) {
        if (startInstant > endInstant) {
            throw new IllegalArgumentException("'startInstant' should not be after 'endInstant'");
        }

        // Are there shorter intervals which overlap with the new interval?
        NavigableMap<Long, Long> view = timeIntervals.subMap(startInstant, true /* inclusive */, endInstant, true /* inclusive */);
        Map.Entry<Long, Long> last = view.pollLastEntry();
        if (last != null) {
            endInstant = Long.max(last.getValue(), endInstant);
        }
        view.clear();

        // Perhaps the end of the interval before 'view' ends after startInstant?
        Map.Entry<Long, Long> floor = timeIntervals.floorEntry(startInstant);
        if (floor != null) {
            long floorEnd = floor.getValue();
            if (floorEnd >= startInstant) {
                timeIntervals.remove(floor.getKey());
                startInstant = floor.getKey();
                endInstant = Long.max(endInstant, floorEnd);
            }
        }

        timeIntervals.put(startInstant, endInstant);
    }

    public boolean contains(long instant) {
        Map.Entry<Long, Long> entry = timeIntervals.floorEntry(instant);
        if (entry == null) {
            return false;
        }
        return instant <= entry.getValue();
    }

    public boolean isEmpty() {
        return timeIntervals.isEmpty();
    }
}
