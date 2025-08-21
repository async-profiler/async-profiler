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

public class OtlpSignalCorrelationTest {
    private static final Duration TEST_DURATION = Duration.ofSeconds(10);

    public static void main(String[] args) throws Exception {
        String traceId = "4bf92f3577b34da6a3ce929d0e0e4736";
        String spanId = "00f067aa0ba902b7";

        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.start(Events.CPU, 1_000_000);

        TraceContext.setTraceContext(traceId, spanId);

        Instant start = Instant.now();
        while (Duration.between(start, Instant.now()).compareTo(TEST_DURATION) < 0) {
            CpuBurner.burn();
        }

        ProfilesData data = dumpAndGetProfile(profiler);
        Files.write(Paths.get("request.txt"), data.toString().getBytes());
        assert data.getDictionary().getLinkTableCount() > 0;

        boolean foundCorrectTrace = false;
        StringBuilder debugInfo = new StringBuilder();
        for (Link link : data.getDictionary().getLinkTableList()) {
             debugInfo.append("Link trace ID: ").append(link.getTraceId()).append("\n");
            if (link.getTraceId().toString().equals(traceId) && link.getSpanId().toString().equals(spanId)) {
                foundCorrectTrace = true;
                break;
            }
        }
        Files.write(Paths.get("debug.txt"), debugInfo.toString().getBytes());
        assert foundCorrectTrace : "not correct traec";
        
        // Profile profile = data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
        // int samplesWithLinks = 0;
        // for (Sample sample : profile.getSampleList()) {
        //     if (sample.getLinkIndex() > 0) {
        //         samplesWithLinks++;
        //     }
        // }
        // assert samplesWithLinks > 0;
        profiler.clearTraceContext();
        profiler.stop();

    }

    public static ProfilesData dumpAndGetProfile(AsyncProfiler profiler) throws Exception {
        byte[] dump = profiler.dumpOtlp();
        ProfilesData data = ProfilesData.parseFrom(dump);
        return data;
    }
}
