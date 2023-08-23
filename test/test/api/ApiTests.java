package test.api;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class ApiTests {

    @Test(mainClass = DumpCollapsed.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void flat(TestProcess p) throws Exception {
        Output out = p.waitForExit("%out");
        assert out.contains("BusyLoops.method1;");
        assert out.contains("BusyLoops.method2;");
        assert out.contains("BusyLoops.method3;");
    }

    @Test(mainClass = StopResume.class, jvmArgs = "-Djava.library.path=build/lib", output = true)
    public void stopResume(TestProcess p) throws Exception {
        Output out = p.waitForExit("%out");
        assert !out.contains("BusyLoops.method1");
        assert out.contains("BusyLoops.method2");
        assert !out.contains("BusyLoops.method3");
    }
}
