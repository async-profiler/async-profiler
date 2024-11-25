/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import one.profiler.test.Assert;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true)
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");
        assert out.ratio("(PlatformEvent::.ark|PlatformEvent::.npark)") > 0.1
                || ((out.ratio("ReentrantLock.lock") + out.ratio("ReentrantLock.unlock")) > 0.1 && out.contains("Unsafe_.ark"));
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.contains("sun/nio/ch/DatagramChannelImpl.send");
    }

    @Test(mainClass = RaceToLock.class, inputs = "0", output = true)
    @Test(mainClass = RaceToLock.class, inputs = "10000", output = true)
    @Test(mainClass = RaceToLock.class, inputs = "1000000", output = true)
    public void raceToLocks(TestProcess p) throws Exception {
        int interval = Integer.parseInt(p.inputs()[0]);
        Output out = p.profile("--lock " + interval + " --threads -o collapsed");
        Output stdout = p.readFile(TestProcess.STDOUT);

        Assert.isGreater(out.samples("\\[random1"), 0, "sampled all threads 1/4");
        Assert.isGreater(out.samples("\\[random2"), 0, "sampled all threads 2/4");
        Assert.isGreater(out.samples("\\[shared1"), 0, "sampled all threads 3/4");
        Assert.isGreater(out.samples("\\[shared2"), 0, "sampled all threads 4/4");

        if (interval == 0) {
            long shared1 = out.samples("\\[shared1");
            long shared2 = out.samples("\\[shared2");
            double sharedDiff = (double) Math.abs(shared1 - shared2) / Math.max(shared1, shared2);
            Assert.isLess(sharedDiff, 0.1, "sharedDiff < 0.1");
        }

        Assert.isGreater(stdout.samples("sharedWaitTime"), stdout.samples("randomWaitTime"), "sharedWaitTime > randomWaitTime");
    }
}
