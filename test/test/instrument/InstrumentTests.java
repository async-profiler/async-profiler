package test.instrument;

import one.profiler.test.*;

public class InstrumentTests {

    @Test(
            mainClass = CpuBurner.class,
            agentArgs = "start,event=test.instrument.CpuBurner.burn,latency=100ms,collapsed,file=%f",
    )
    public void testCompTask(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert p.exitCode() == 0;

        assert out.contains("test/instrument/CpuBurner.lambda$main$0;test/instrument/CpuBurner.burn 1");
        assert out.contains("test/instrument/CpuBurner.lambda$main$1;test/instrument/CpuBurner.burn 2");
    }

}
