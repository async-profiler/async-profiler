package test.smoke;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class SmokeTests {

    @Test(mainClass = Cpu.class)
    public void cpu(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        out.assertContains("test/smoke/Cpu.main;test/smoke/Cpu.method1");
        out.assertContains("test/smoke/Cpu.main;test/smoke/Cpu.method2");
        out.assertContains("test/smoke/Cpu.main;test/smoke/Cpu.method3;java/io/File");
    }

    @Test(mainClass = Alloc.class)
    public void alloc(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e alloc -o collapsed -t");
        out.assertContains("\\[AllocThread-1 tid=[0-9]+];.*Alloc.allocate;.*java.lang.Integer\\[]");
        out.assertContains("\\[AllocThread-2 tid=[0-9]+];.*Alloc.allocate;.*int\\[]");
    }

    @Test(mainClass = Threads.class, agentArgs = "start,event=cpu,collapsed,threads,file=%f")
    public void threads(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        out.assertContains("\\[ThreadEarlyEnd tid=[0-9]+];.*Threads.methodForThreadEarlyEnd;.*");
        out.assertContains("\\[RenamedThread tid=[0-9]+];.*Threads.methodForRenamedThread;.*");
    }

    @Test(mainClass = LoadLibrary.class)
    public void loadLibrary(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms");
        Output out = p.readFile("%f");
        out.assertContains("Java_sun_management");
    }
}
