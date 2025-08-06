import io.opentelemetry.api.GlobalOpenTelemetry;
import io.opentelemetry.api.trace.Span;
import io.opentelemetry.api.trace.Tracer;
import io.opentelemetry.context.Scope;
import io.opentelemetry.sdk.OpenTelemetrySdk;
import io.opentelemetry.sdk.trace.SdkTracerProvider;

public class TestApp {
    public static void main(String[] args) {
        SdkTracerProvider tracerProvider = SdkTracerProvider.builder().build();
        OpenTelemetrySdk.builder()
            .setTracerProvider(tracerProvider)
            .buildAndRegisterGlobal();
        
        Tracer tracer = GlobalOpenTelemetry.getTracer("test-app");
        
        for (int i = 0; i < 3; i++) {
            final int threadId = i;
            new Thread(() -> {
                Span span = tracer.spanBuilder("cpu-work-" + threadId).startSpan();
                try (Scope scope = span.makeCurrent()) {
                    System.out.println("[tid=" + Thread.currentThread().getId() + "] trace ID: " + 
                        span.getSpanContext().getTraceId() + ", span ID: " + 
                        span.getSpanContext().getSpanId());
                    doWork();
                } finally {
                    span.end();
                }
            }).start();
        }
    }
    
    private static void doWork() {
        while (true) {
            for (int i = 0; i < 10_000_000; i++) {
                Math.sqrt(i);
            }
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                break;
            }
        }
    }
}
