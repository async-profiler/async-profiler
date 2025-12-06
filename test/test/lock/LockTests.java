/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true, jvmVer = {11, Integer.MAX_VALUE})
    public void datagramSocketLock(TestProcess p) throws Exception {
        checkCpu(p);

        Output out = p.profile("-e lock -d 3 -o collapsed");
        assert out.contains("sun/nio/ch/DatagramChannelImpl.send");
    }

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true, jvmVer = {11, Integer.MAX_VALUE})
    public void datagramSocketLockLoop(TestProcess p) throws Exception {
        checkCpu(p);

        p.profile("start -e lock --loop 1s");
        Thread.sleep(2_000);
        Output out = p.profile("stop -o collapsed");
        assert out.contains("sun/nio/ch/DatagramChannelImpl.send");
    }

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true, jvmVer = {11, Integer.MAX_VALUE})
    public void datagramSocketLockStopStart(TestProcess p) throws Exception {
        p.profile("start -e lock");
        Thread.sleep(2_000);
        Output out1 = p.profile("stop -o collapsed");
        assert out1.contains("sun/nio/ch/DatagramChannelImpl.send");

        p.profile("start -e lock");
        Thread.sleep(2_000);
        Output out2 = p.profile("stop -o collapsed");
        assert out2.contains("sun/nio/ch/DatagramChannelImpl.send");
    }

    private static void checkCpu(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");
        assert out.ratio("(ReentrantLock.lock|ReentrantLock.unlock)") > 0.1;
        assert out.contains("ReentrantLock.lock");
        assert out.contains("ReentrantLock.unlock");
        assert out.contains("Unsafe.park");
        assert out.contains("Unsafe.unpark");
    }
}
