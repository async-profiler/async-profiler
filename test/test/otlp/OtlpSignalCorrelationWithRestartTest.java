/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.TraceContext;
import io.opentelemetry.proto.profiles.v1development.*;
import static test.otlp.OtlpSignalCorrelationTest.*;

public class OtlpSignalCorrelationWithRestartTest {
    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        
        Thread longSpanThread = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_1, SPAN_ID_1);

            for (int i = 0; i < 3; i++) {
                burnCpu();
            }
            
        }, "LongSpan");
        
        profiler.execute("start,otlp,threads");
        longSpanThread.start();
        
        Thread.sleep(1000);
        profiler.stop();
        profiler.execute("start,otlp,threads");
        
        longSpanThread.join();
        
        ProfilesData data = dumpAndGetProfile(profiler);
        profiler.stop();
        
        assert data.getDictionary().getLinkTableCount() == 1 : data.getDictionary().getLinkTableList().toString();
        
    }
}
