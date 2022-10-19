package linkage;

import java.io.File;
import java.nio.file.Path;
import org.junit.jupiter.api.Test;

import one.profiler.AsyncProfiler;
import java.util.concurrent.ThreadLocalRandom;

public class AsyncProfilerLinkageTest {

    @Test
    public void testContextApiLinked() throws Exception {
        AsyncProfiler ap = AsyncProfiler.getInstance(getAsyncProfilerLib());
        ap.addThread(Thread.currentThread());
        ap.setContext(1, 1);
        ap.clearContext();
    }

    private static String getAsyncProfilerLib() throws Exception {
        File root = new File(AsyncProfilerLinkageTest.class
            .getResource("AsyncProfilerLinkageTest.class").toURI()).getParentFile();
        while (!root.getName().equals("async-profiler")) {
            root = root.getParentFile();
        }
        return root.toPath().resolve("build/libasyncProfiler.so").toAbsolutePath().toString();
    }
}
