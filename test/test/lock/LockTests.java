package test.lock;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true)
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed --cstack dwarf"); 
        assert (((out.ratio("Unsafe..ark") + out.ratio("Unsafe..npark")) > 0.1)||((out.ratio("PlatformEvent::.ark") + out.ratio("PlatformEvent::.npark")) > 0.1));

        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.samples("sun/nio/ch/DatagramChannelImpl.send") > 10;
    }
}
