/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.TraceContext;
import io.opentelemetry.proto.profiles.v1development.*;
import io.opentelemetry.proto.common.v1.AnyValue;
import java.util.Optional;
import java.util.concurrent.CountDownLatch;
import static test.otlp.OtlpSignalCorrelationTest.*;

public class OtlpSignalCorrelationWithRestartTest {
    private static final String TRACE_ID_1 = "09a6c61f1181ce4fc439e5728c5fef75";
    private static final String SPAN_ID_1 = "b98c89ad5d208dcc";
    private static final String TRACE_ID_2 = "adf1a04340a7a3995571db46c3c648dc";
    private static final String SPAN_ID_2 = "86dd157fc2677b88";

   public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        CountDownLatch restartLatch = new CountDownLatch(1);
        
        Thread continuousThread = new Thread(() -> {
            try {
                TraceContext.setTraceContext(TRACE_ID_1, SPAN_ID_1);
                burnCpu();
                restartLatch.await();

                TraceContext.setTraceContext(TRACE_ID_1, SPAN_ID_1);
                burnCpu();
            } catch (Exception e) {
                throw new RuntimeException("Span thread failed during restart", e);
            }
        }, "ContinuousSpan");
        
        profiler.execute("start,otlp,threads");
        continuousThread.start();
        
        Thread.sleep(500);
        
        profiler.stop();
        profiler.execute("start,otlp,threads");
        
        restartLatch.countDown();
        
        Thread newThread = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_2, SPAN_ID_2);
            burnCpu();
        }, "NewSpanAfterRestart");
        
        newThread.start();
        continuousThread.join();
        newThread.join();
        
        ProfilesData data = dumpAndGetProfile(profiler);
        profiler.stop();
        
        assert data.getDictionary().getLinkTableCount() == 3 : data.getDictionary().getLinkTableList().toString();
        
        boolean foundTrace1 = false, foundTrace2 = false;
        Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        
        for (Sample sample : profile.getSampleList()) {
            if (sample.getLinkIndex() >= 0 && sample.getLinkIndex() < data.getDictionary().getLinkTableCount()) {
                Optional<AnyValue> threadNameOpt = OtlpTests.getAttribute(sample, data.getDictionary(), "thread.name");
                if (threadNameOpt.isPresent()) {
                    String threadName = threadNameOpt.get().getStringValue();
                    
                    Link link = data.getDictionary().getLinkTable(sample.getLinkIndex());
                    String linkTrace = bytesToHex(link.getTraceId());
                    String linkSpan = bytesToHex(link.getSpanId());
                    
                    if (threadName.equals("ContinuousSpan")) {
                        assert (linkTrace.equals("") && linkSpan.equals("")) || 
                            (linkTrace.equals(TRACE_ID_1) && linkSpan.equals(SPAN_ID_1));
                        if (linkTrace.equals(TRACE_ID_1)) foundTrace1 = true;
                    } else if (threadName.equals("NewSpanAfterRestart")) {
                        assert (linkTrace.equals("") && linkSpan.equals("")) || 
                            (linkTrace.equals(TRACE_ID_2) && linkSpan.equals(SPAN_ID_2));
                        if (linkTrace.equals(TRACE_ID_2)) foundTrace2 = true;
                    } else {
                        assert linkTrace.equals("") && linkSpan.equals("");
                    }
                }
            }
        }
        
        assert foundTrace1 : "TRACE_ID_1 not found";
        assert foundTrace2 : "TRACE_ID_2 not found";
    }

}
