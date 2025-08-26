/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class MethodTrace extends Event {
    private final String method;
    private final long duration;

    public MethodTrace(long time, int tid, int stackTraceId, String method, long duration) {
        super(time, tid, stackTraceId);
        this.method = method;
        this.duration = duration;
    }

    public String method() {
        return method;
    }

    public long duration() {
        return duration;
    }
}
