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
    private static final Duration TEST_DURATION = Duration.ofSeconds(3);

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.start(Events.CPU, 1_000_000);

        Thread[] threads = new Thread[3];
        
        threads[0] = new Thread(() -> {
            TraceContext.setTraceContext("09a6c61f1181ce4fc439e5728c5fef75", "b98c89ad5d208dcc");
            burnCpu();
        });
        
        threads[1] = new Thread(() -> {
            TraceContext.setTraceContext("adf1a04340a7a3995571db46c3c648dc", "86dd157fc2677b88");
            burnCpu();
        });
        
        threads[2] = new Thread(() -> {
            TraceContext.setTraceContext("761d3a2cbae70d2e625985c887874266", "839dfc1575e1ea94");
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
        
        assert data.getDictionary().getLinkTableCount() >= 3;
        
        Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        int samplesWithLinks = 0;
        for (Sample sample : profile.getSampleList()) {
            if (sample.getLinkIndex() > 0) {
                samplesWithLinks++;
            }
        }
        assert samplesWithLinks > 0;
        
        Map<String, String> expectedPairs = new HashMap<>();
        expectedPairs.put("09a6c61f1181ce4fc439e5728c5fef75", "b98c89ad5d208dcc");
        expectedPairs.put("adf1a04340a7a3995571db46c3c648dc", "86dd157fc2677b88");
        expectedPairs.put("761d3a2cbae70d2e625985c887874266", "839dfc1575e1ea94");
        boolean allLinksValid = true;
        StringBuilder debugOutput = new StringBuilder();
        for (int i = 1; i < data.getDictionary().getLinkTableCount(); i++) {
            Link link = data.getDictionary().getLinkTable(i);
            
            String linkTrace = bytesToHex(link.getTraceId());
            String linkSpan = bytesToHex(link.getSpanId());
            
            if (!expectedPairs.containsKey(linkTrace) || 
                !expectedPairs.get(linkTrace).equals(linkSpan)) {
                allLinksValid = false;
                break;
            }
        }
        assert allLinksValid;
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
        for (byte b : bytes.toByteArray()) {
            hex.append(String.format("%02x", b & 0xFF));
        }
        return hex.toString();
    }

}
