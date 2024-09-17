/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {
    private static void contendedLocks(TestProcess p, int interval, double minRatio) throws Exception {
        Output out = p.profile("--lock " + interval + " --threads -o collapsed");
        Output waitingLocks = out.filter("LockProfiling\\.contend;java\\.lang\\.Object");

        double ratio = 100 * out.ratio("contend-0.5-", "contend-0.05-");

        System.out.println("interval = " + interval + ", ratio = " + ratio + ", minRatio = " + minRatio);

        assert (ratio >= minRatio) || (Double.isNaN(ratio) && Double.isNaN(minRatio));
    }

    // 0 is equivalent to disabling sampling of locks, so all profiles are included.
    @Test(mainClass = LockProfiling.class, inputs = { "0", "70" })
    @Test(mainClass = LockProfiling.class, inputs = { "10000", "70" })

    // Large (for the specific paylod) interval value skews the sampled lock
    // contention distribution.
    @Test(mainClass = LockProfiling.class, inputs = { "1000000", "90" })

    // Very large interval causes all profiles be dropped.
    @Test(mainClass = LockProfiling.class, inputs = { "1000000000", "NaN" })
    public void contendedLocks(TestProcess p) throws Exception {
        int interval = Integer.parseInt(p.inputs()[0]);
        double minRatio = Double.parseDouble(p.inputs()[1]);

        contendedLocks(p, interval, minRatio);
    }

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true) // Fails on Alpine
    public void datagramSocketLock(TestProcess p) throws Exception {
        boolean dwarfUnwind = !System.getProperty("os.arch").equals("ppc64le");
        Output out = p.profile("-e cpu -d 3 -o collapsed" + (dwarfUnwind ? " --cstack dwarf" : ""));
        assert out.ratio("(PlatformEvent::.ark|PlatformEvent::.npark)") > 0.1
                || (out.ratio("ReentrantLock.lock") > 0.1 && out.contains("Unsafe_.ark"));
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.samples("sun/nio/ch/DatagramChannelImpl.send") > 10;
    }
}
