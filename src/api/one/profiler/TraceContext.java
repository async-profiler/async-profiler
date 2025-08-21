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
        byte[] traceBytes = traceId.getBytes(StandardCharsets.UTF_8);
        buffer.put(traceBytes, 0, traceBytes.length);
        
        byte[] spanBytes = spanId.getBytes(StandardCharsets.UTF_8);
        buffer.put(spanBytes, 0, spanBytes.length);
    }

    public static void clearTraceContext() {
        ByteBuffer buffer = BUFFER.get();
        if (buffer == null) return;
        
        buffer.position(0);
        for (int i = 0; i < 48; i++) {
            buffer.put((byte) 0);
        }
    }
}
