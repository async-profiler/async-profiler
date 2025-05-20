/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.stackwalker;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class StackwalkerTests {

    private static final String FRAME = "([^\\[;]+;)";
    private static final String OPTIONAL_FRAME = FRAME + "?";

    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "largeFrame",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr", nameSuffix = "VMX")
    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "largeFrame",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr", nameSuffix = "VM")
    public void largeFrame(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^\\[possibly-truncated\\];" +
                "Java_test_stackwalker_StackGenerator_generateLargeFrame;" +
                "doCpuTask");
    }

    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "deepStack",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr", nameSuffix = "VMX")
    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "deepStack",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr", nameSuffix = "VM")
    public void deepStack(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^\\[possibly-truncated\\];" +
                "Java_test_stackwalker_StackGenerator_generateDeepStack;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "doCpuTask");
    }

    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "completeStack",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr")
    public void normalStackVMX(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^" +
                FRAME + // Root can be different on different systems
                OPTIONAL_FRAME + // It's possible to have an additional thread frame depending on the OS
                "ThreadJavaMain;" +
                "JavaMain;" +
                "(invokeStaticMainWithArgs;)?" + // Added in newer JDK versions => 24
                "jni_CallStaticVoidMethod;" +
                "jni_invoke_static;" +
                "JavaCalls::call_helper;" +
                "call_stub;" +
                "test/stackwalker/StackGenerator.main_\\[0\\];" +
                "test/stackwalker/StackGenerator.generateCompleteStack_\\[0\\];" +
                "Java_test_stackwalker_StackGenerator_generateCompleteStack;" +
                "doCpuTask");
    }

    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "completeStack",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr")
    public void normalStackVM(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^test/stackwalker/StackGenerator.main_\\[0\\];" +
                "test/stackwalker/StackGenerator.generateCompleteStack_\\[0\\];" +
                "Java_test_stackwalker_StackGenerator_generateCompleteStack;" +
                "doCpuTask");
    }
}
