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
    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true) // Fails on Alpine
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");
        assert out.ratio("(PlatformEvent::.ark|PlatformEvent::.npark)") > 0.1
                || (out.ratio("ReentrantLock.lock") > 0.1 && out.contains("Unsafe_.ark"));
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.samples("sun/nio/ch/DatagramChannelImpl.send") > 10;
    }

    @Test(mainClass = RaceToLock.class, inputs = { "0" }, args = "1000", output = true)
    @Test(mainClass = RaceToLock.class, inputs = { "10000" }, args = "1000", output = true)
    @Test(mainClass = RaceToLock.class, inputs = { "1000000000" }, args = "1000", output = true)
    public void raceToLocks(TestProcess p) throws Exception {
        int interval = Integer.parseInt(p.inputs()[0]);

        p.profile("--lock " + interval + " --threads -o collapsed");
        Output stdout = p.readFile(TestProcess.STDOUT);

        Assert.isGreater(stdout.samples("sharedWaitTime"), stdout.samples("randomWaitTime"), "sharedWaitTime > randomWaitTime");
    }
}
