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
        buffer.put(traceId.getBytes(StandardCharsets.UTF_8));
        buffer.put(spanId.getBytes(StandardCharsets.UTF_8));
    }

    public static void clearTraceContext() {
        ByteBuffer buffer = BUFFER.get();
        if (buffer == null) return;
        buffer.clear();
    }
}
