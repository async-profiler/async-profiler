/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public abstract class IntervalEvent extends Event {
    public final long duration;

    protected IntervalEvent(long time, int tid, int stackTraceId, long duration) {
        super(time, tid, stackTraceId);
        this.duration = duration;
    }

    @Override
    public long value() {
        return duration;
    }

    public String tag() {
        return null;
    }
}
