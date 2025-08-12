import io.opentelemetry.context.Context;
import io.opentelemetry.sdk.trace.ReadWriteSpan;
import io.opentelemetry.sdk.trace.ReadableSpan;
import io.opentelemetry.sdk.trace.SpanProcessor;
import one.profiler.AsyncProfiler;

public class AsyncProfilerSpanProcessor implements SpanProcessor {
    @Override
    public void onStart(Context parentContext, ReadWriteSpan span) {
        String traceId = span.getSpanContext().getTraceId();
        String spanId = span.getSpanContext().getSpanId();
        
        AsyncProfiler.getInstance().setTraceContext(traceId, spanId);
        
        System.out.println("[PROCESSOR tid=" + Thread.currentThread().getId() + 
            "] trace ID: " + traceId + ", span ID: " + spanId);
    }

    @Override
    public void onEnd(ReadableSpan span) {
        AsyncProfiler.getInstance().clearTraceContext();
    }

    @Override
    public boolean isStartRequired() {
        return true;
    }

    @Override
    public boolean isEndRequired() {
        return true;
    }
}