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
        writeString(buffer, traceId);
        writeString(buffer, spanId);
    }

    public static void clearTraceContext() {
        ByteBuffer buffer = BUFFER.get();
        if (buffer == null) return;
        
        buffer.clear();
        }

    private static void writeString(ByteBuffer buffer, String str) {
        byte[] bytes = str.getBytes(StandardCharsets.UTF_8);
        buffer.put(bytes);
    }
}
