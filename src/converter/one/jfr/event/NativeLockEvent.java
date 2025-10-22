/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class NativeLockEvent extends Event {
    public final long address;
    public final long duration;

    public NativeLockEvent(long time, int tid, int stackTraceId, long address, long duration) {
        super(time, tid, stackTraceId);
        this.address = address;
        this.duration = duration;
    }

    @Override
    public long value() {
        return duration;
    }
}
