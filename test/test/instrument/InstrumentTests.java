package test.instrument;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class InstrumentTests {

    @Test(mainClass = JavaProperties.class, agentArgs = "start,event=java.util.Properties.getProperty,collapsed,file=%f", nameSuffix = "default")
    @Test(mainClass = JavaProperties.class, agentArgs = "start,event=java.util.Properties.getProperty,collapsed,cstack=vm,file=%f", nameSuffix = "VM")
    @Test(mainClass = JavaProperties.class, agentArgs = "start,event=java.util.Properties.getProperty,collapsed,cstack=vmx,file=%f", nameSuffix = "VMX")
    public void instrument(TestProcess p) throws Exception {
        Output output = p.waitForExit("%f");

        output.stream().forEach(line -> {
            assert line.matches(".*java/util/Properties.getProperty [0-9]+") : line;
        });
    }
}
