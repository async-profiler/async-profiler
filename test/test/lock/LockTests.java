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
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");
        assert out.ratio("(ReentrantLock.lock|ReentrantLock.unlock)") > 0.1;
        assert out.contains("ReentrantLock.lock");
        assert out.contains("ReentrantLock.unlock");
        assert out.contains("Unsafe.park");
        assert out.contains("Unsafe.unpark");
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.contains("sun/nio/ch/DatagramChannelImpl.send");
    }
}
