/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

import one.jfr.JfrReader;

public class CPUTimeSample extends Event implements IThreadState {
    public final boolean failed;
    public final long samplingPeriod;
    public final boolean biased;

    public CPUTimeSample(long time, int tid, int stackTraceId, boolean failed, long samplingPeriod, boolean biased) {
        super(time, tid, stackTraceId);
        this.failed = failed;
        this.samplingPeriod = samplingPeriod;
        this.biased = biased;
    }

    @Override public final int threadState() {
        // CPUTimeSample are always from RUNNABLE state.
        return 5;
    }
}
