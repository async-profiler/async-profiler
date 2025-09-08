/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class MethodTrace extends Event {
    public final int method;
    public final long duration;

    public MethodTrace(long time, int tid, int stackTraceId, int method, long duration) {
        super(time, tid, stackTraceId);
        this.method = method;
        this.duration = duration;
    }

    @Override
    public int hashCode() {
        return method * 127 + stackTraceId;
    }

    @Override
    public boolean sameGroup(Event o) {
        if (o instanceof MethodTrace) {
            MethodTrace c = (MethodTrace) o;
            return method == c.method;
        }
        return false;
    }

    @Override
    public long value() {
        return duration;
    }
}
