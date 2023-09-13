package test.api;

import one.profiler.test.Output;
import one.profiler.test.OAssert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class ApiTests {

    @Test(mainClass = DumpCollapsed.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void flat(TestProcess p) throws Exception {
        Thread.sleep(1000);
        Output out = p.waitForExit("%pout");
        Thread.sleep(1000);
        OAssert.contains(out, "BusyLoops.method1;");
        OAssert.contains(out, "BusyLoops.method2;");
        OAssert.contains(out, "BusyLoops.method3;");
    }

    @Test(mainClass = StopResume.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void stopResume(TestProcess p) throws Exception {
        Thread.sleep(1000);
        Output out = p.waitForExit("%pout");
        Thread.sleep(1000);
        OAssert.notContains(out, "BusyLoops.method1");
        OAssert.contains(out, "BusyLoops.method2");
        OAssert.notContains(out, "BusyLoops.method3");
    }
}
