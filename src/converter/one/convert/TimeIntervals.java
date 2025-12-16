/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.util.Map;
import java.util.NavigableMap;
import java.util.TreeMap;
import java.util.Arrays;

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

    public TimeIntervalsList asList() {
        long[] startIntervals = new long[timeIntervals.size()];
        long[] endIntervals = new long[timeIntervals.size()];
        int index = 0;
        for (Map.Entry<Long, Long> entry : timeIntervals.entrySet()) {
            startIntervals[index] = entry.getKey();
            endIntervals[index] = entry.getValue();
            ++index;
        }
        return new TimeIntervalsList(startIntervals, endIntervals);
    }

    public static final class TimeIntervalsList {
        private final long[] startIntervals;
        private final long[] endIntervals;

        public TimeIntervalsList(long[] startIntervals, long[] endIntervals) {
            this.startIntervals = startIntervals;
            this.endIntervals = endIntervals;
        }

        public boolean contains(long instant) {
            int searchOut = Arrays.binarySearch(startIntervals, instant);
            if (searchOut >= 0) {
                return true;
            }

            int insertionPoint = -(searchOut + 1); // First element greater than instant
            if (insertionPoint == 0) {
                return false; // First interval start is greater than instant
            }
            int startIndex = insertionPoint - 1;
            return instant <= endIntervals[startIndex];
        }
    }
}
