package test.recovery;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;
import one.profiler.test.ArchType;

public class RecoveryTests {

    @Test(mainClass = StringBuilderTest.class, arch = {ArchType.X64, ArchType.X86}, debugNonSafepoints = true)
    public void stringBuilder(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");

        out.assertRatioGreater("StringBuilder.delete;", 0.9);
        out.assertRatioGreater("arraycopy", 0.9);
        out.assertRatioLess("unknown_Java", 0.01);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        out.assertRatioLess("StringBuilder.delete;", 0.1);
        out.assertRatioGreater("unknown_Java", 0.5);
    }

    @Test(mainClass = StringBuilderTest.class, debugNonSafepoints = true, arch = {ArchType.ARM64, ArchType.ARM32})
    public void stringBuilderArm(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        out.assertRatioGreater("(forward|foward|backward)_copy_longs", 0.9); //there's a typo on some JDK versions

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 2");
        out.assertRatioLess("StringBuilder.delete;", 0.1);
        out.assertRatioGreater("unknown_Java", 0.5);
    }

    @Test(mainClass = Numbers.class, debugNonSafepoints = true)
    public void numbers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        out.assertRatioGreater("vtable stub", 0.01);
        out.assertRatioGreater("Numbers.loop", 0.8);

        out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        out.assertRatioGreater("unknown_Java", 0.1);
    }

    @Test(mainClass = Suppliers.class, debugNonSafepoints = true)
    public void suppliers(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed --safe-mode 31");
        out.assertRatioGreater("unknown_Java", 0.2);

        out = p.profile("-d 3 -e cpu -o collapsed");
        out.assertRatioGreater("itable stub", 0.01);
        out.assertRatioGreater("Suppliers.loop" ,0.6);
    }
}
