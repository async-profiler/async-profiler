/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class ExecutionSample extends Event {
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
