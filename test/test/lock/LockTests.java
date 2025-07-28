/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import one.profiler.test.Musl;
import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    // TODO: MUSL has problems in unwinding native stacks due to missing frame pointers & eh_frame section
    //  this in turn makes the test unstable on MUSL as sometime no sample is able to unwind to find the "Unsafe_Park" method
    //  https://github.com/async-profiler/async-profiler/issues/1317 & https://github.com/async-profiler/async-profiler/issues/1320
    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true, musl = Musl.NOT_MUSL)
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");

        assert out.ratio("(PlatformEvent::.ark|PlatformEvent::.npark)") > 0.1
                || ((out.ratio("ReentrantLock.lock") + out.ratio("ReentrantLock.unlock")) > 0.1 && out.contains("Unsafe_Park"));
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.contains("sun/nio/ch/DatagramChannelImpl.send");
    }
}
