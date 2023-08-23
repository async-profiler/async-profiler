package test.kernel;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class KernelTests {

    @Test(mainClass = ListFiles.class)
    public void kernel(TestProcess p) throws Exception {
        Output out = p.profile("-e cpu -d 3 -i 1ms -o collapsed");
        assert out.contains("test/kernel/ListFiles.listFiles;java/io/File");
        assert out.contains("sys_getdents");

        out = p.profile("stop -o flamegraph");
        assert out.contains("f\\(\\d+,\\d+,\\d+,0,'java/io/File.list'\\)");
        assert out.contains("f\\(\\d+,\\d+,\\d+,2,'.*sys_getdents'\\)");
    }

    @Test(mainClass = ListFiles.class)
    public void fdtransfer(TestProcess p) throws Exception {
        p.profile("-e cpu -d 3 -i 1ms -o collapsed -f %f --fdtransfer", true);
        Output out = p.readFile("%f");
        assert out.contains("test/kernel/ListFiles.listFiles;java/io/File");
        assert out.contains("sys_getdents");
    }
}
