/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true)
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");

        boolean nativeSample = out.contains("Unsafe_Park") || (out.contains("Unsafe_Unpark") && p.musl());
        assert out.ratio("(PlatformEvent::.ark|PlatformEvent::.npark)") > 0.1
                || ((out.ratio("ReentrantLock.lock") + out.ratio("ReentrantLock.unlock")) > 0.1 && nativeSample);
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.contains("sun/nio/ch/DatagramChannelImpl.send");
    }
}
