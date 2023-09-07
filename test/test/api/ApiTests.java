package test.api;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class ApiTests {

    @Test(mainClass = DumpCollapsed.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void flat(TestProcess p) throws Exception {
        Output out = p.waitForExit("%out");
        out.assertContains("BusyLoops.method1;");
        out.assertContains("BusyLoops.method2;");
        out.assertContains("BusyLoops.method3;");
    }

    @Test(mainClass = StopResume.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void stopResume(TestProcess p) throws Exception {
        Output out = p.waitForExit("%out");
        out.assertNotContains("BusyLoops.method1");
        out.assertContains("BusyLoops.method2");
        out.assertNotContains("BusyLoops.method3");
    }
}
