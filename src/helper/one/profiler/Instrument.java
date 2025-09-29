/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

/**
 * Instrumentation helper for Java method profiling.
 */
public class Instrument {

    private Instrument() {
    }

    public static native void recordEntry();

    public static void recordExit(long startTimeNs, long minLatency) {
        if (System.nanoTime() - startTimeNs >= minLatency) {
            recordExit0(startTimeNs);
        }
    }

    // Overload used when latency=0, we don't call recordExit0
    // directly to have the same number of additional frames as
    // the standard path.
    public static void recordExit(long startTimeNs) {
        recordExit0(startTimeNs);
    }

    public static native void recordExit0(long startTimeNs);
}
