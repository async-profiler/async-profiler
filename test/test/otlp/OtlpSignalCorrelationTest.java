/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.TraceContext;
import io.opentelemetry.proto.profiles.v1development.*;
import java.time.Duration;
import java.time.Instant;
import com.google.protobuf.ByteString;
import io.opentelemetry.proto.common.v1.AnyValue;
import io.opentelemetry.proto.common.v1.KeyValue;
import java.util.Optional;

public class OtlpSignalCorrelationTest {
    private static final Duration TEST_DURATION = Duration.ofSeconds(1);

    private static final String TRACE_ID_1 = "09a6c61f1181ce4fc439e5728c5fef75";
    private static final String SPAN_ID_1 = "b98c89ad5d208dcc";
    private static final String TRACE_ID_1_ALT = "55812e1a0e7d80817be621dedec6accb";
    private static final String SPAN_ID_1_ALT = "ef95eccf36472d99";
    private static final String TRACE_ID_2 = "adf1a04340a7a3995571db46c3c648dc";
    private static final String SPAN_ID_2 = "86dd157fc2677b88";
    private static final String TRACE_ID_3 = "761d3a2cbae70d2e625985c887874266";
    private static final String SPAN_ID_3 = "839dfc1575e1ea94";

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.execute("start,otlp,threads");

        Thread[] threads = new Thread[3];
        
        threads[0] = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_1, SPAN_ID_1);
            burnCpu();
            TraceContext.setTraceContext(TRACE_ID_1_ALT, SPAN_ID_1_ALT);
            burnCpu();
        }, "CorrelationThread1");

        threads[1] = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_2, SPAN_ID_2);
            burnCpu();
        }, "CorrelationThread2");

        threads[2] = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_3, SPAN_ID_3);
            burnCpu();
        }, "CorrelationThread3");

        for (Thread t : threads) {
            t.start();
        }
        
        for (Thread t : threads) {
            t.join();
        }

        ProfilesData data = dumpAndGetProfile(profiler);
        profiler.stop();
        
        assert data.getDictionary().getLinkTableCount() == 5 : 
            "Expected 5 links in link_table, but got: " + data.getDictionary().getLinkTableCount();
        
        Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        int samplesWithLinks = 0;

        for (Sample sample : profile.getSampleList()) {
            if (sample.getLinkIndex() > 0) {
                samplesWithLinks++;
            }
        }
        assert samplesWithLinks > 4;

        boolean foundTrace1 = false, foundTrace1Alt = false, foundTrace2 = false, foundTrace3 = false;
        
        for (Sample sample : profile.getSampleList()) {
            if (sample.getLinkIndex() >= 0) {
                Optional<AnyValue> threadNameOpt = OtlpTests.getAttribute(sample, data.getDictionary(), "thread.name");
                if (threadNameOpt.isPresent()) {
                    String threadName = threadNameOpt.get().getStringValue();
                    
                    Link link = data.getDictionary().getLinkTable(sample.getLinkIndex());
                    String linkTrace = bytesToHex(link.getTraceId());
                    String linkSpan = bytesToHex(link.getSpanId());

                    if (linkTrace.equals(TRACE_ID_1)) foundTrace1 = true;
                    else if (linkTrace.equals(TRACE_ID_1_ALT)) foundTrace1Alt = true;
                    else if (linkTrace.equals(TRACE_ID_2)) foundTrace2 = true;
                    else if (linkTrace.equals(TRACE_ID_3)) foundTrace3 = true;
                    
                    if (threadName.equals("CorrelationThread1")) {
                        assert (linkTrace.equals("") && linkSpan.equals("")) ||
                            (linkTrace.equals(TRACE_ID_1) && linkSpan.equals(SPAN_ID_1)) ||
                            (linkTrace.equals(TRACE_ID_1_ALT) && linkSpan.equals(SPAN_ID_1_ALT));
                    } else if (threadName.equals("CorrelationThread2")) {
                        assert (linkTrace.equals("") && linkSpan.equals("")) ||
                            (linkTrace.equals(TRACE_ID_2) && linkSpan.equals(SPAN_ID_2));
                    } else if (threadName.equals("CorrelationThread3")) {
                        assert (linkTrace.equals("") && linkSpan.equals("")) ||
                            (linkTrace.equals(TRACE_ID_3) && linkSpan.equals(SPAN_ID_3));
                    } else {
                        assert linkTrace.equals("") && linkSpan.equals("");
                    }
                }
            }
        }

        assert foundTrace1 : "TRACE_ID_1 not found";
        assert foundTrace1Alt : "TRACE_ID_1_ALT not found";
        assert foundTrace2 : "TRACE_ID_2 not found";
        assert foundTrace3 : "TRACE_ID_3 not found";

    }

    private static void burnCpu() {
        Instant start = Instant.now();
        while (Duration.between(start, Instant.now()).compareTo(TEST_DURATION) < 0) {
            CpuBurner.burn();
        }
    }

    public static ProfilesData dumpAndGetProfile(AsyncProfiler profiler) throws Exception {
        byte[] dump = profiler.dumpOtlp();
        ProfilesData data = ProfilesData.parseFrom(dump);
        return data;
    }

    private static String bytesToHex(ByteString bytes) {
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < bytes.size(); ++i) {
            hex.append(String.format("%02x", bytes.byteAt(i) & 0xFF));
        }
        return hex.toString();
    }

}
