package test.lock;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class LockTests {

    @Test(mainClass = DatagramTest.class, debugNonSafepoints = true)
    public void datagramSocketLock(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -o collapsed");
        assert out.ratio("pthread_cond_signal") > 0.1;

        out = p.profile("-e lock -d 3 -o collapsed");
        assert out.samples("sun/nio/ch/DatagramChannelImpl.send") > 10;
    }
}
