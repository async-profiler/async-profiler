/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Span {

    private static final ThreadLocal<ByteBuffer> LAST_SAMPLE = new ThreadLocal<ByteBuffer>() {
        @Override
        protected ByteBuffer initialValue() {
            try {
                return AsyncProfiler.getThreadLocalBuffer().order(ByteOrder.nativeOrder());
            } catch (UnsatisfiedLinkError e) {
                return ByteBuffer.allocate(8).order(ByteOrder.nativeOrder());
            }
        }
    };

    private final long startTime;
    private final String tag;

    public Span(String tag) {
        this.startTime = System.nanoTime();
        this.tag = tag;
    }

    public void commit() {
        long lastSampleTime = LAST_SAMPLE.get().getLong(0);
        if (lastSampleTime - startTime > 0) {
            AsyncProfiler.emitSpan(startTime, System.nanoTime(), tag);
        }
    }
}
