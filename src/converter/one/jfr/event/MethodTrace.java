/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class MethodTrace extends Event {
    private final int method;
    private final long duration;

    public MethodTrace(long time, int tid, int stackTraceId, int method, long duration) {
        super(time, tid, stackTraceId);
        this.method = method;
        this.duration = duration;
    }

    @Override
    public int hashCode() {
        int result = 17;
        result = 31 * result + time;
        result = 31 * result + tid;
        return result;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof MethodTrace)) {
            return false;
        }

        MethodTrace m = (MethodTrace) o;
        return m.time == time && m.tid == m.tid;
    }
}
