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
        assert output.contains("^Java_test_stackwalker_StackGenerator_largeFrame;" +
                "doCpuTask");

        // There will be no stack frame that contains main method, largeFrame native method & doCpuTask
        assert !output.contains(".*main.*Java_test_stackwalker_StackGenerator_largeFrame.*doCpuTask");
    }

    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "deepFrame",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr", nameSuffix = "VMX")
    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "deepFrame",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr", nameSuffix = "VM")
    public void deepStack(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^Java_test_stackwalker_StackGenerator_deepFrame;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "generateDeepStack[^;]*;" +
                "doCpuTask");

        // There will be no stack frame that contains main method, deepFrame native method & doCpuTask
        assert !output.contains(".*main.*Java_test_stackwalker_StackGenerator_deepFrame.*doCpuTask");
    }

    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "leafFrame",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr")
    public void normalStackVMX(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^" +
                FRAME +          // Platform-dependent root frame
                OPTIONAL_FRAME + // Platform-dependent
                OPTIONAL_FRAME + // ThreadJavaMain frame could be missing in JDK 8
                "JavaMain;" +
                OPTIONAL_FRAME + // JDK 22 added "invokeStaticMainWithArgs" function
                "jni_CallStaticVoidMethod;" +
                "jni_invoke_static;" +
                "JavaCalls::call_helper;" +
                "call_stub;" +
                "test/stackwalker/StackGenerator.main_\\[0\\];" +
                "test/stackwalker/StackGenerator.leafFrame_\\[0\\];" +
                "Java_test_stackwalker_StackGenerator_leafFrame;" +
                "doCpuTask");
    }

    @Test(mainClass = StackGenerator.class, jvmArgs = "-Xss5m", args = "leafFrame",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr")
    public void normalStackVM(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^test/stackwalker/StackGenerator.main_\\[0\\];" +
                "test/stackwalker/StackGenerator.leafFrame_\\[0\\];" +
                "Java_test_stackwalker_StackGenerator_leafFrame;" +
                "doCpuTask");
    }

    @Test(mainClass = StackGenForUnwindViaDebugFrame.class, jvmArgs = "-Xss5m",
            agentArgs = "start,event=cpu,cstack=vm,file=%f.jfr", os = Os.LINUX)
    public void walkStackUsingDebugFrameVM(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^" +
                "test/stackwalker/StackGenForUnwindViaDebugFrame.main_\\[0\\];" +
                "test/stackwalker/StackGenForUnwindViaDebugFrame.startWork_\\[0\\];" +
                "Java_test_stackwalker_StackGenForUnwindViaDebugFrame_startWork;" +
                "doRecursiveWork[^;]*;" +
                "doRecursiveWork[^;]*;" +
                "doRecursiveWork[^;]*;" +
                "doRecursiveWork[^;]*;" +
                "useCpu");
    }

    @Test(mainClass = StackGenForUnwindViaDebugFrame.class, jvmArgs = "-Xss5m",
            agentArgs = "start,event=cpu,cstack=vmx,file=%f.jfr", os = Os.LINUX)
    public void walkStackUsingDebugFrameVMX(TestProcess p) throws Exception {
        p.waitForExit();
        assert p.exitCode() == 0;
        Output output = Output.convertJfrToCollapsed(p.getFilePath("%f"));
        assert output.contains("^" +
                FRAME +          // Platform-dependent root frame
                OPTIONAL_FRAME + // Platform-dependent
                OPTIONAL_FRAME + // ThreadJavaMain frame could be missing in JDK 8
                "JavaMain;" +
                OPTIONAL_FRAME + // JDK 22 added "invokeStaticMainWithArgs" function
                "jni_CallStaticVoidMethod;" +
                "jni_invoke_static;" +
                "JavaCalls::call_helper;" +
                "call_stub;"+
                "test/stackwalker/StackGenForUnwindViaDebugFrame.main_\\[0\\];" +
                "test/stackwalker/StackGenForUnwindViaDebugFrame.startWork_\\[0\\];" +
                "Java_test_stackwalker_StackGenForUnwindViaDebugFrame_startWork;" +
                "doRecursiveWork[^;]*;" +
                "doRecursiveWork[^;]*;" +
                "doRecursiveWork[^;]*;" +
                "doRecursiveWork[^;]*;" +
                "useCpu");
    }
}
