package test.control;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class ControlTests {
    @Test(sh = "%testbin/start_control %f1.jfr", env = {"ASPROF_COMMAND=start,cpu,file=%f2.jfr", "LD_LIBRARY_PATH=build/lib"}, nameSuffix="fake-preload")
    @Test(sh = "%testbin/start_control %f1.jfr", env = {"ASPROF_COMMAND=start,cpu,file=%f2.jfr", "LD_LIBRARY_PATH=build/lib"}, nameSuffix="normal")
    public void earlyStart(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;

        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f1"), "--nativemem");
        assert output.contains("main;malloc_hook");
    }
}
