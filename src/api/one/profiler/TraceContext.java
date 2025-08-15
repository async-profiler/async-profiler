package one.profiler;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

public class TraceContext {
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
        writeString(buffer, traceId, 32);
        writeString(buffer, spanId, 16);
        buffer.put((byte) 0);
    }

    public static void clearTraceContext() {
        ByteBuffer buffer = BUFFER.get();
        if (buffer == null) return;
        
        buffer.position(0);
        for (int i = 0; i < 49; i++) {
            buffer.put((byte) 0);
        }
    }

    private static void writeString(ByteBuffer buffer, String str, int len) {
        if (str == null) throw new IllegalArgumentException("String cannot be null");
        if (str.length() < len) throw new IllegalArgumentException("String too long");
        byte[] bytes = str.getBytes(StandardCharsets.UTF_8);
        buffer.put(bytes, 0, bytes.length);
        for (int i = bytes.length; i < len; i++) buffer.put((byte) 0);
    }
}