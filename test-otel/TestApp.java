import io.opentelemetry.api.GlobalOpenTelemetry;
import io.opentelemetry.api.trace.Span;
import io.opentelemetry.api.trace.Tracer;
import io.opentelemetry.context.Scope;
import io.opentelemetry.sdk.OpenTelemetrySdk;
import io.opentelemetry.sdk.trace.SdkTracerProvider;

public class TestApp {
    public static void main(String[] args) {
        System.out.println("Creating SpanProcessor");
        
        SdkTracerProvider tracerProvider = SdkTracerProvider.builder()
            .addSpanProcessor(new CustomSpanProcessor())
            .build();
        
        System.out.println("Building OpenTelemetry");
        
        OpenTelemetrySdk.builder()
            .setTracerProvider(tracerProvider)
            .buildAndRegisterGlobal();

        System.out.println("Getting tracer");
        Tracer tracer = GlobalOpenTelemetry.getTracer("test-app");

        System.out.println("Starting threads");
        for (int i = 0; i < 3; i++) {
            final int threadId = i;
            new Thread(() -> {
                System.out.println("Thread " + threadId + " starting span");
                Span span = tracer.spanBuilder("cpu-work-" + threadId).startSpan();
                try (Scope scope = span.makeCurrent()) {
                    System.out.println("Thread " + threadId + " doing work");
                    doWork();
                } finally {
                    System.out.println("Thread " + threadId + " ending span");
                    span.end();
                }
            }).start();
        }
        try { Thread.sleep(5000); } catch (InterruptedException e) {}
    }

    private static void doWork() {
        for (int i = 0; i < 10_000_000; i++) {
            Math.sqrt(i);
        }
    }
}