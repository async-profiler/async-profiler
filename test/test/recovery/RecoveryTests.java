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

        Assert.isGreater(out.ratio("StringBuilder.delete;"), 0.8);
        Assert.isGreater(out.ratio("arraycopy"), 0.8);
        Assert.isLess(out.ratio("unknown_Java"), 0.01);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        Assert.isLess(out.ratio("StringBuilder.delete;"), 0.1);
        Assert.isGreater(out.ratio("unknown_Java"), 0.5);

        out = p.profile("-d 2 -e cpu -i 1ms --cstack vm -o collapsed");
        Assert.isGreater(out.ratio("StringBuilderTest.main;java/lang/StringBuilder.delete;"), 0.8);
        Assert.isLess(out.ratio("unknown|break_compiled"), 0.002);
    }

    @Test(mainClass = StringBuilderTest.class, debugNonSafepoints = true, arch = {Arch.ARM64, Arch.ARM32})
    public void stringBuilderArm(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.isGreater(out.ratio("(forward|foward|backward)_copy_longs"), 0.8); // there's a typo on some JDK versions

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        Assert.isLess(out.ratio("StringBuilder.delete;"), 0.1);
        Assert.isGreater(out.ratio("unknown_Java"), 0.5);

        out = p.profile("-d 2 -e cpu -i 1ms --cstack vm -o collapsed");
        Assert.isGreater(out.ratio("StringBuilderTest.main;java/lang/StringBuilder.delete;"), 0.8);
        Assert.isLess(out.ratio("unknown|break_compiled"), 0.002);
    }

    @Test(mainClass = Numbers.class, debugNonSafepoints = true)
    public void numbers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.isGreater(out.ratio("vtable stub"), 0.01);
        Assert.isGreater(out.ratio("Numbers.loop"), 0.8);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        Assert.isGreater(out.ratio("unknown_Java"), 0.05);

        out = p.profile("-d 2 -e cpu -i 1ms --cstack vm -o collapsed");
        Assert.isGreater(out.ratio("Numbers.main;[^;]+;test/recovery/Numbers.avg"), 0.8);
        Assert.isLess(out.ratio("unknown|break_compiled"), 0.002);
    }

    @Test(mainClass = Suppliers.class, debugNonSafepoints = true)
    public void suppliers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        Assert.isGreater(out.ratio("unknown_Java"), 0.2);

        out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.isGreater(out.ratio("itable stub"), 0.01);
        Assert.isGreater(out.ratio("Suppliers.loop"), 0.5);

        out = p.profile("-d 2 -e cpu -i 1ms --cstack vm -o collapsed");
        Assert.isGreater(out.ratio("Suppliers.main;test/recovery/Suppliers.loop"), 0.5);
        Assert.isLess(out.ratio("unknown|break_compiled"), 0.002);
    }

    @Test(mainClass = CodingIntrinsics.class, debugNonSafepoints = true, arch = {Arch.ARM64, Arch.X64}, inputs = "")
    @Test(mainClass = CodingIntrinsics.class, debugNonSafepoints = true, arch = {Arch.ARM64, Arch.X64}, inputs = "--cstack vm", nameSuffix = "VM")
    public void intrinsics(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -i 1ms -o collapsed " + p.inputs()[0]);
        Assert.isLess(out.ratio("^\\[unknown"), 0.02, "No more than 2% of unknown frames");
        Assert.isLess(out.ratio("^[^ ;]+(;[^ ;]+)? "), 0.02, "No more than 2% of short stacks");
    }
}
