/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.recovery;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.Arch;

public class RecoveryTests {

    @Test(mainClass = StringBuilderTest.class, jvmArgs = "-XX:UseAVX=2", arch = {Arch.X64, Arch.X86}, debugNonSafepoints = true)
    public void stringBuilder(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");

        Assert.isGreater(out.ratio("StringBuilder.delete;"), 0.9);
        Assert.isGreater(out.ratio("arraycopy"), 0.9);
        Assert.isLess(out.ratio("unknown_Java"), 0.01);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        Assert.isLess(out.ratio("StringBuilder.delete;"), 0.1);
        Assert.isGreater(out.ratio("unknown_Java"), 0.5);
    }

    @Test(mainClass = StringBuilderTest.class, debugNonSafepoints = true, arch = {Arch.ARM64, Arch.ARM32})
    public void stringBuilderArm(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.isGreater(out.ratio("(forward|foward|backward)_copy_longs"), 0.8); // there's a typo on some JDK versions

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        Assert.isLess(out.ratio("StringBuilder.delete;"), 0.1);
        Assert.isGreater(out.ratio("unknown_Java"), 0.5);
    }

    @Test(mainClass = Numbers.class, debugNonSafepoints = true)
    public void numbers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.isGreater(out.ratio("vtable stub"), 0.01);
        Assert.isGreater(out.ratio("Numbers.loop"), 0.8);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        Assert.isGreater(out.ratio("unknown_Java"), 0.1);
    }

    @Test(mainClass = Suppliers.class, debugNonSafepoints = true)
    public void suppliers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        Assert.isGreater(out.ratio("unknown_Java"), 0.2);

        out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.isGreater(out.ratio("itable stub"), 0.01);
        Assert.isGreater(out.ratio("Suppliers.loop"), 0.5);
    }
}
