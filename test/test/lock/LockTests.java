package test.lock;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true) // Fails on Alpine
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf");
        try {
            assert out.ratio("(PlatformEvent::.ark|PlatformEvent::.npark)") > 0.1 || (out.ratio("ReentrantLock.lock") > 0.1 && out.contains("Unsafe_.ark"));
            //"(Unsafe_.ark|Unsafe_.npark|PlatformEvent::.ark|PlatformEvent::.npark)", 0.1;
        } catch (AssertionError e){
            throw new AssertionError("Expected (PlatformEvent::.ark|PlatformEvent::.npark) ratio < 0.1\n Received out may be available in build/test/logs.");
        }
        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.samples("sun/nio/ch/DatagramChannelImpl.send") > 10;
    }
}
