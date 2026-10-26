/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.wall;

import one.jfr.JfrReader;
import one.jfr.event.ExecutionSample;
import one.profiler.test.Assert;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

import java.util.List;

public class WallTests {

    @Test(mainClass = SocketTest.class)
    public void cpuWall(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed");
        Assert.isGreater(out.ratio("test/wall/SocketTest.main"), 0.25);
        Assert.isGreater(out.ratio("test/wall/BusyClient.run"), 0.25);
        Assert.isLess(out.ratio("test/wall/IdleClient.run"), 0.05);

        out = p.profile("-e wall -d 3 -o collapsed");
        long s1 = out.samples("test/wall/SocketTest.main");
        long s2 = out.samples("test/wall/BusyClient.run");
        long s3 = out.samples("test/wall/IdleClient.run");
        assert s1 > 10 && s2 > 10 && s3 > 10;
        assert Math.abs(s1 - s2) < 5 && Math.abs(s2 - s3) < 5 && Math.abs(s3 - s1) < 5;
    }

    // Regression test for #1249
    @Test(mainClass = WallFlushApp.class, agentArgs = "start,wall=50ms,jfr,file=%f.jfr")
    public void flushBufferedSleepingSamples(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        int sleepingSamples = 0;
        try (JfrReader jfr = new JfrReader(p.getFilePath("%f"))) {
            jfr.stopAtNewChunk = true;
            List<ExecutionSample> events = jfr.readAllEvents(ExecutionSample.class);
            int sleepingState = jfr.getEnumKey("jdk.types.ThreadState", "STATE_SLEEPING");
            assert sleepingState >= 0 : "STATE_SLEEPING enum not found in JFR";
            for (ExecutionSample e : events) {
                if (e.threadState == sleepingState
                        && "WallFlushSleeper".equals(jfr.threads.get(e.tid))) {
                    sleepingSamples += e.samples;
                }
            }
        }

        Assert.isGreater(sleepingSamples, 5);
    }
}
