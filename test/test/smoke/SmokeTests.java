package test.smoke;

import one.profiler.test.Output;
import one.profiler.test.Assert;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class SmokeTests {

    @Test(mainClass = Cpu.class)
    public void cpu(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        Assert.contains(out, "test/smoke/Cpu.main;test/smoke/Cpu.method1");
        Assert.contains(out, "test/smoke/Cpu.main;test/smoke/Cpu.method2");
        Assert.contains(out, "test/smoke/Cpu.main;test/smoke/Cpu.method3;java/io/File");
    }

    @Test(mainClass = Alloc.class)
    public void alloc(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e alloc -o collapsed -t");
        Assert.contains(out, "\\[AllocThread-1 tid=[0-9]+];.*Alloc.allocate;.*java.lang.Integer\\[]");
        Assert.contains(out, "\\[AllocThread-2 tid=[0-9]+];.*Alloc.allocate;.*int\\[]");
    }

    @Test(mainClass = Threads.class, agentArgs = "start,event=cpu,collapsed,threads,file=%f")
    public void threads(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        Assert.contains(out, "\\[ThreadEarlyEnd tid=[0-9]+];.*Threads.methodForThreadEarlyEnd;.*");
        Assert.contains(out, "\\[RenamedThread tid=[0-9]+];.*Threads.methodForRenamedThread;.*");
    }

    @Test(mainClass = LoadLibrary.class)
    public void loadLibrary(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms");
        Output out = p.readFile("%f");
        Assert.contains(out, "Java_sun_management");
    }
}
