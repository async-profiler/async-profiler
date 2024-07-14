/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.smoke;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class SmokeTests {

    @Test(mainClass = Cpu.class)
    public void cpu(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e cpu -o collapsed");
        assert out.contains("test/smoke/Cpu.main;test/smoke/Cpu.method1");
        assert out.contains("test/smoke/Cpu.main;test/smoke/Cpu.method2");
        assert out.contains("test/smoke/Cpu.main;test/smoke/Cpu.method3;java/io/File");
    }

    @Test(mainClass = Alloc.class)
    public void alloc(TestProcess p) throws Exception {
        Output out = p.profile("-d 3 -e alloc -o collapsed -t");
        assert out.contains("\\[AllocThread-1 tid=[0-9]+];.*Alloc.allocate;.*java.lang.Integer\\[]");
        assert out.contains("\\[AllocThread-2 tid=[0-9]+];.*Alloc.allocate;.*int\\[]");
    }

    @Test(mainClass = Threads.class, agentArgs = "start,event=cpu,collapsed,threads,file=%f")
    public void threads(TestProcess p) throws Exception {
        Output out = p.waitForExit("%f");
        assert out.contains("\\[ThreadEarlyEnd tid=[0-9]+];.*Threads.methodForThreadEarlyEnd;.*");
        assert out.contains("\\[RenamedThread tid=[0-9]+];.*Threads.methodForRenamedThread;.*");
    }

    @Test(mainClass = LoadLibrary.class)
    public void loadLibrary(TestProcess p) throws Exception {
        p.profile("-f %f -o collapsed -d 4 -i 1ms");
        Output out = p.readFile("%f");
        assert out.contains("Java_sun_management");
    }
}
