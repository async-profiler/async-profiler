/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.Events;
import io.opentelemetry.proto.profiles.v1development.*;

public class OtlpTemporalityTest {

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.start(Events.CPU, 1_000_000);

        Profile profile1 = dumpAndGetProfile(profiler);
        long timeNanos1 = profile1.getTimeNanos();
        long durationNanos1 = profile1.getDurationNanos();
        
        Thread.sleep(100);
        
        Profile profile2 = dumpAndGetProfile(profiler);
        long timeNanos2 = profile2.getTimeNanos();
        long durationNanos2 = profile2.getDurationNanos();
        
        assert timeNanos1 == timeNanos2;
        assert durationNanos2 - durationNanos1 >= 100_000_000L;
        
        profiler.stop();
        profiler.start(Events.CPU, 1_000_000);
        
        Profile profile3 = dumpAndGetProfile(profiler);
        long timeNanos3 = profile3.getTimeNanos();
        
        assert timeNanos3 > timeNanos1;
        
        profiler.stop();
    }

    public static Profile dumpAndGetProfile(AsyncProfiler profiler) throws Exception {
        byte[] dump = profiler.dumpOtlp();
        ProfilesData data = ProfilesData.parseFrom(dump);
        return data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
    }
}
