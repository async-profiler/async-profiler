// import datadog.trace.api.Trace;
// import datadog.trace.bootstrap.instrumentation.api.AgentScope;
// import datadog.trace.bootstrap.instrumentation.api.AgentSpan;
// import datadog.trace.bootstrap.instrumentation.api.AgentTracer;
// import datadog.trace.bootstrap.instrumentation.api.ScopeSource;

import one.profiler.AsyncProfiler;
import java.util.concurrent.ThreadLocalRandom;

public class ContextTarget {
    private static final AsyncProfiler ap;

    static {
        ap = AsyncProfiler.getInstance();
    }

    public static void main(String[] args) {
        int tid = ap.getNativeThreadId();
        ap.addThread(tid);
        long ts = System.nanoTime();
        entry(200, 750);
        System.err.println("===> time: " + (System.nanoTime() - ts) + "ns");
        ap.removeThread(tid);
    }

    // @Trace(operationName = "doEntry")
    private static void entry(int top, int lower) {
        int cumulative = 0;
        for (int i = 0; i < top; i++) {
            cumulative += doOp2(lower);
            if ((i + 1) % 40 == 0) {
                System.out.println("===> " + cumulative);
            }
        }
    }

    private static int doOp2(int lower) {
        int val = 0;
        for (int i = 0; i < lower; i++) {
            long spanId = ThreadLocalRandom.current().nextLong();
            long rootSpanId = ThreadLocalRandom.current().nextLong();

            ap.setContext(spanId, rootSpanId);
            try {
                for (int j = 0; j < 100; j++) {
                    val += ThreadLocalRandom.current().nextInt();
                    Thread.yield();
                }
            } finally {
                ap.clearContext();
            }
        }
        return val;
    }
}
