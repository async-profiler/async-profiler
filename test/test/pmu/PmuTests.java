package test.pmu;

import one.profiler.test.Output;
import one.profiler.test.Test;
import one.profiler.test.TestProcess;

public class PmuTests {

    @Test(mainClass = Dictionary.class)
    public void cycles(TestProcess p) throws Exception {
        p.profile("-e cycles -d 3 -o collapsed -f %f");
        Output out = p.readFile("%f");
        assert out.ratio("test/pmu/Dictionary.test128K") > 0.4;
        assert out.ratio("test/pmu/Dictionary.test8M") > 0.4;
    }

    @Test(mainClass = Dictionary.class)
    public void cacheMisses(TestProcess p) throws Exception {
        p.profile("-e cache-misses -d 3 -o collapsed -f %f");
        Output out = p.readFile("%f");
        assert out.ratio("test/pmu/Dictionary.test128K") < 0.2;
        assert out.ratio("test/pmu/Dictionary.test8M") > 0.8;
    }
}
