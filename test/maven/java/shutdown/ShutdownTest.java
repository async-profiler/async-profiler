package shutdown;

import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.Queue;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.LinkedBlockingQueue;

import one.profiler.AsyncProfiler;
import utils.Utils;

import static org.junit.jupiter.api.Assertions.fail;

public class ShutdownTest {


  @Test
  public void testShutdownCpu() throws IOException {
    AsyncProfiler ap = AsyncProfiler.getInstance(Utils.getAsyncProfilerLib());
    runTest(ap, "start,cpu=10us,filter=0,thread");
  }

  @Test
  public void testShutdownWall() throws IOException {
    AsyncProfiler ap = AsyncProfiler.getInstance(Utils.getAsyncProfilerLib());
    ap.addThread(Thread.currentThread());
    runTest(ap, "start,wall=~10us,filter=0,thread");
  }

  @Test
  public void testShutdownCpuAndWall() throws IOException {
    AsyncProfiler ap = AsyncProfiler.getInstance(Utils.getAsyncProfilerLib());
    ap.addThread(Thread.currentThread());
    runTest(ap, "start,cpu=10us,wall=~10us,filter=0,thread");
  }

  private static void runTest(AsyncProfiler asyncProfiler, String command) throws IOException {
    Path jfrDump = Files.createTempFile("filter-test", ".jfr");
    String commandWithDump = command + ",jfr,file=" + jfrDump.toAbsolutePath();
    ExecutorService executor = Executors.newSingleThreadExecutor();
    Queue<Throwable> errors = new LinkedBlockingQueue<>();
    try {
      executor.submit(new Runnable() {
        @Override
        public void run() {
          for (int i = 0; i < 100; i++) {
            try {
              asyncProfiler.execute(commandWithDump);
              try {
                Thread.sleep(1);
              } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
              }
              asyncProfiler.stop();
            } catch (Throwable error) {
              errors.offer(error);
              return;
            }
          }
        }
      }).get();
    } catch (Throwable t) {
      fail(t.getMessage());
    } finally {
      executor.shutdownNow();
    }
    if (!errors.isEmpty()) {
      fail(errors.poll());
    }
  }
}
