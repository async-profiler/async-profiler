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

public class OtlpSignalCorrelationTest {
    private static final Duration TEST_DURATION = Duration.ofSeconds(3);

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.start(Events.CPU, 1_000_000);

        Thread[] threads = new Thread[3];
        
        threads[0] = new Thread(() -> {
            TraceContext.setTraceContext("4bf92f3577b34da6a3ce929d0e0e4736", "00f067aa0ba902b7");
            burnCpu();
        });
        
        threads[1] = new Thread(() -> {
            TraceContext.setTraceContext("1234567890abcdef1234567890abcdef", "fedcba0987654321");
            burnCpu();
        });
        
        threads[2] = new Thread(() -> {
            TraceContext.setTraceContext("abcdefabcdefabcdefabcdefabcdefab", "1122334455667788");
            burnCpu();
        });

        for (Thread t : threads) {
            t.start();
        }
        
        for (Thread t : threads) {
            t.join();
        }

        ProfilesData data = dumpAndGetProfile(profiler);
        
        assert data.getDictionary().getLinkTableCount() >= 3;
        
        Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        int samplesWithLinks = 0;
        for (Sample sample : profile.getSampleList()) {
            if (sample.getLinkIndex() > 0) {
                samplesWithLinks++;
            }
        }
        assert samplesWithLinks > 0;
        
        profiler.stop();
        System.out.println("SUCCESS: Found " + samplesWithLinks + " samples with trace correlation " + 
                        (data.getDictionary().getLinkTableCount() - 1) + " different traces");
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
}
