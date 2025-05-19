/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stackwalker;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class StackwalkerTests {

    @Test(mainClass = Stackwalker.class, jvmArgs = "-Xss5m", args = "walkStackLargeFrame",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr", nameSuffix = "VMX")
    @Test(mainClass = Stackwalker.class, jvmArgs = "-Xss5m", args = "walkStackLargeFrame",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr", nameSuffix = "VM")
    public void largeFrame(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^\\[possible-truncated\\];" +
                "Java_test_stackwalker_Stackwalker_walkStackLargeFrame;" +
                "doCpuTask");
        assert !output.contains("\\[possible-truncated\\].*Stackwalker.main");
    }

    @Test(mainClass = Stackwalker.class, jvmArgs = "-Xss5m", args = "walkStackDeepStack",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr", nameSuffix = "VMX")
    @Test(mainClass = Stackwalker.class, jvmArgs = "-Xss5m", args = "walkStackDeepStack",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr", nameSuffix = "VM")
    public void deepStack(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^\\[possible-truncated\\];" +
                "Java_test_stackwalker_Stackwalker_walkStackDeepStack;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "doCpuTask");
        assert !output.contains("\\[possible-truncated\\].*Stackwalker.main");
    }

    @Test(mainClass = Stackwalker.class, jvmArgs = "-Xss5m", args = "walkStackComplete",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr")
    public void normalStackVMX(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^([^\\[;]+;)?" + // Root can be different on different systems
                "(start_thread;|thread_start;)?" + // Mac Vs Linux
                "(_pthread_start;)?" + // Mac specific frame
                "ThreadJavaMain;" +
                "JavaMain;" +
                "jni_CallStaticVoidMethod;" +
                "jni_invoke_static;" +
                "JavaCalls::call_helper;" +
                "call_stub;" +
                "test/stackwalker/Stackwalker.main_\\[0\\];" +
                "test/stackwalker/Stackwalker.walkStackComplete_\\[0\\];" +
                "Java_test_stackwalker_Stackwalker_walkStackComplete;" +
                "doCpuTask");
        assert !output.contains("\\[possible-truncated\\].*Stackwalker.main");
    }

    @Test(mainClass = Stackwalker.class, jvmArgs = "-Xss5m", args = "walkStackComplete",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr")
    public void normalStackVM(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^test/stackwalker/Stackwalker.main_\\[0\\];" +
                "test/stackwalker/Stackwalker.walkStackComplete_\\[0\\];" +
                "Java_test_stackwalker_Stackwalker_walkStackComplete;" +
                "doCpuTask");
        assert !output.contains("break_entry_frame");
    }
}
