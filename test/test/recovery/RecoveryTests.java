package test.recovery;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.ArchType;

public class RecoveryTests {

    @Test(mainClass = StringBuilderTest.class, arch = {ArchType.X64, ArchType.X86}, debugNonSafepoints = true)
    public void stringBuilder(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");

        Assert.ratioGreater(out, "StringBuilder.delete;", 0.9);
        Assert.ratioGreater(out, "arraycopy", 0.9);
        Assert.ratioLess(out, "unknown_Java", 0.01);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        Assert.ratioLess(out, "StringBuilder.delete;", 0.1);
        Assert.ratioGreater(out, "unknown_Java", 0.5);
    }

    @Test(mainClass = StringBuilderTest.class, debugNonSafepoints = true, arch = {ArchType.ARM64, ArchType.ARM32})
    public void stringBuilderArm(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.ratioGreater(out, "(forward|foward|backward)_copy_longs", 0.9); //there's a typo on some JDK versions

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        Assert.ratioLess(out, "StringBuilder.delete;", 0.1);
        Assert.ratioGreater(out, "unknown_Java", 0.5);
    }

    @Test(mainClass = Numbers.class, debugNonSafepoints = true)
    public void numbers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.ratioGreater(out, "vtable stub", 0.01);
        Assert.ratioGreater(out, "Numbers.loop", 0.8);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        Assert.ratioGreater(out, "unknown_Java", 0.1);
    }

    @Test(mainClass = Suppliers.class, debugNonSafepoints = true)
    public void suppliers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        Assert.ratioGreater(out, "unknown_Java", 0.2);

        out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.ratioGreater(out, "itable stub", 0.01);
        Assert.ratioGreater(out, "Suppliers.loop", 0.6);
    }
}
