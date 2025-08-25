/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.Events;
import one.profiler.TraceContext;
import io.opentelemetry.proto.profiles.v1development.*;
import java.time.Duration;
import java.time.Instant;
import java.nio.file.Files;
import java.nio.file.Paths;
import com.google.protobuf.ByteString;
import java.util.Map;
import java.util.HashMap;
import java.nio.charset.StandardCharsets;

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
        profiler.execute("start,otlp");

        Thread[] threads = new Thread[3];
        
        threads[0] = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_1, SPAN_ID_1);
            burnCpu();
            TraceContext.setTraceContext(TRACE_ID_1_ALT, SPAN_ID_1_ALT);
            burnCpu();
        });

        threads[1] = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_2, SPAN_ID_2);
            burnCpu();
        });

        threads[2] = new Thread(() -> {
            TraceContext.setTraceContext(TRACE_ID_3, SPAN_ID_3);
            burnCpu();
        });

        for (Thread t : threads) {
            t.start();
        }
        
        for (Thread t : threads) {
            t.join();
        }

        ProfilesData data = dumpAndGetProfile(profiler);
        profiler.stop();
        
        assert data.getDictionary().getLinkTableCount() == 5 : 
            data.getDictionary().getLinkTableCount();
        
        Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        int samplesWithLinks = 0;
        for (Sample sample : profile.getSampleList()) {
            if (sample.getLinkIndex() > 0) {
                samplesWithLinks++;
            }
        }
        assert samplesWithLinks > 3;
        
        Map<String, String> expectedPairs = new HashMap<>();
        expectedPairs.put("", "");
        expectedPairs.put(TRACE_ID_1, SPAN_ID_1);
        expectedPairs.put(TRACE_ID_1_ALT, SPAN_ID_1_ALT);
        expectedPairs.put(TRACE_ID_2, SPAN_ID_2);
        expectedPairs.put(TRACE_ID_3, SPAN_ID_3);
        boolean allLinksValid = true;
        for (int i = 0; i < data.getDictionary().getLinkTableCount(); i++) {
            Link link = data.getDictionary().getLinkTable(i);
            
            String linkTrace = bytesToHex(link.getTraceId());
            String linkSpan = bytesToHex(link.getSpanId());
            
            assert expectedPairs.get(linkTrace).equals(linkSpan);
        }
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
