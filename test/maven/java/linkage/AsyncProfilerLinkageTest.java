package linkage;

import org.junit.jupiter.api.Test;

import one.profiler.AsyncProfiler;
import utils.Utils;

public class AsyncProfilerLinkageTest {

    @Test
    public void testContextApiLinked() throws Exception {
        AsyncProfiler ap = AsyncProfiler.getInstance(Utils.getAsyncProfilerLib());
        ap.addThread(ap.getNativeThreadId());
        ap.setContext(1, 1);
        ap.clearContext();
    }
}
