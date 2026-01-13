/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
package test.otlp;

import one.profiler.AsyncProfiler;
import one.profiler.Events;
import io.opentelemetry.proto.profiles.v1development.*;

public class OtlpProfileTimeTest {

    public static void main(String[] args) throws Exception {
        AsyncProfiler profiler = AsyncProfiler.getInstance();
        profiler.start(Events.CPU, 1_000_000);

        Profile profile1 = dumpAndGetProfile(profiler);
        long timeNano1 = profile1.getTimeUnixNano();
        long durationNano1 = profile1.getDurationNano();

        Thread.sleep(100);

        Profile profile2 = dumpAndGetProfile(profiler);
        long timeNano2 = profile2.getTimeUnixNano();
        long durationNano2 = profile2.getDurationNano();

        assert timeNano1 == timeNano2 : String.format("%d, %d", timeNano1, timeNano2);
        assert durationNano2 - durationNano1 >= 100_000_000L : String.format("%d, %d", durationNano2, durationNano1);

        profiler.stop();
        profiler.start(Events.CPU, 1_000_000);

        Profile profile3 = dumpAndGetProfile(profiler);
        long timeNano3 = profile3.getTimeUnixNano();

        assert timeNano3 > timeNano1 : String.format("%d, %d", timeNano3, timeNano1);

        profiler.stop();
    }

    public static Profile dumpAndGetProfile(AsyncProfiler profiler) throws Exception {
        byte[] dump = profiler.dumpOtlp();
        ProfilesData data = ProfilesData.parseFrom(dump);
        return data.getResourceProfiles(0).getScopeProfiles(0).getProfiles(0);
    }
}
