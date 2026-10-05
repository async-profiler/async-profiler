/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package one.jfr.event;

public class MappingEvent extends Event {
    public final long address;
    public final long size;
    public final boolean unmap;
    public MappingEvent(long time, int tid, int stackTraceId, long address, long size, boolean unmap) {
        super(time, tid, stackTraceId);
        this.address = address;
        this.size = size;
        this.unmap = unmap;
    }
    @Override public long value() { return size; }
}
