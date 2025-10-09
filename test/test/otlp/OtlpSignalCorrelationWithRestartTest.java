/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.TraceContext;
import io.opentelemetry.proto.profiles.v1development.*;
import static test.otlp.OtlpSignalCorrelationTest.*;
import java.time.Duration;

public class OtlpSignalCorrelationWithRestartTest {
    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        
        Thread longSpanThread = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_1, SPAN_ID_1);

            long start = System.nanoTime();
            Duration duration = Duration.ofSeconds(2);
            while (System.nanoTime() - start < duration.toNanos()) {
                CpuBurner.burn();
            }
            
        }, "LongSpan");
        
        profiler.execute("start,otlp,threads");
        longSpanThread.start();
        
        Thread.sleep(500);
        profiler.stop();
        profiler.execute("start,otlp,threads");
        
        longSpanThread.join();
        
        ProfilesData data = dumpAndGetProfile(profiler);
        profiler.stop();
        
        assert data.getDictionary().getLinkTableCount() == 1 : data.getDictionary().getLinkTableList().toString();
    }
}
