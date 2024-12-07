/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.jfr.event;

public class MallocEvent extends Event {
    public final long address;
    public final long size;

    public MallocEvent(long time, int tid, int stackTraceId, long address, long size) {
        super(time, tid, stackTraceId);
        this.address = address;
        this.size = size;
    }

    @Override
    public long value() {
        return size;
    }
}
