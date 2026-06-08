/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class MethodTrace extends IntervalEvent {
    public final int method;

    public MethodTrace(long time, int tid, int stackTraceId, long duration, int method) {
        super(time, tid, stackTraceId, duration);
        this.method = method;
    }

    @Override
    public int hashCode() {
        return method * 127 + stackTraceId;
    }

    @Override
    public boolean sameGroup(Event o) {
        if (o instanceof MethodTrace) {
            MethodTrace m = (MethodTrace) o;
            return method == m.method;
        }
        return false;
    }
}
