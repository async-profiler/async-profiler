/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class ExecutionSample extends Event {
    // Synthetic thread state to distinguish samples converted from jdk.CPUTimeSample event.
    // A small constant suitable for BitSet, does not clash with any existing thread state.
    public static final int CPU_TIME_SAMPLE = 254;

    public final int threadState;
    public final int samples;

    public ExecutionSample(long time, int tid, int stackTraceId, int threadState, int samples) {
        super(time, tid, stackTraceId);
        this.threadState = threadState;
        this.samples = samples;
    }

    @Override
    public long samples() {
        return samples;
    }

    @Override
    public long value() {
        return samples;
    }
}
