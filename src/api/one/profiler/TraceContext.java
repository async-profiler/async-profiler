/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;

public class TraceContext {
    private static final int TRACE_CONTEXT_SIZE = 24;

    private static final ThreadLocal<ByteBuffer> BUFFER = new ThreadLocal<ByteBuffer>() {
        @Override
        protected ByteBuffer initialValue() {
            try {
                return AsyncProfiler.getThreadLocalBuffer();
            } catch (UnsatisfiedLinkError e) {
                return null;
            }
        }
    };

    public static void setTraceContext(String traceId, String spanId) {
        ByteBuffer buffer = BUFFER.get();
        if (buffer == null) return;

        buffer.position(0);
        putHexString(buffer, traceId);
        putHexString(buffer, spanId);
        // Control byte
        buffer.put((byte) 1);
    }

    public static void clearTraceContext() {
        ByteBuffer buffer = BUFFER.get();
        if (buffer == null) return;
        
        buffer.position(0);
        for (int i = 0; i < TRACE_CONTEXT_SIZE + 1; i++) {
            buffer.put((byte) 0);
        }
    }

    private static void putHexString(ByteBuffer buffer, String hex) {
        for (int i = 0; i < hex.length(); i += 2) {
            int high = Character.digit(hex.charAt(i), 16);
            int low = Character.digit(hex.charAt(i + 1), 16);
            buffer.put((byte) ((high << 4) + low));
        }
    }
    
}
