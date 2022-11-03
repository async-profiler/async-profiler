package filter;

import one.profiler.AsyncProfiler;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;

public class ThreadFilterSmokeTest {
  private ExecutorService executorService;

  @BeforeEach
  public void before() {
    executorService = Executors.newCachedThreadPool();
  }

  @AfterEach
  public void after() {
    executorService.shutdownNow();
  }

  @Test
  public void smokeTest() throws Exception {
    Path jfrDump = Files.createTempFile("filter-test", ".jfr");
    AsyncProfiler asyncProfiler = AsyncProfiler.getInstance();
    doThreadFiltering(asyncProfiler);
    asyncProfiler.execute("start,wall=~1ms,filter=0,jfr,thread,file=" + jfrDump.toAbsolutePath());
    doThreadFiltering(asyncProfiler);
  }

  private void doThreadFiltering(AsyncProfiler asyncProfiler) throws Exception {
    Future<?>[] futures = new Future[1000];
    for (int i = 0; i < futures.length; i++) {
      int id = i;
      futures[i] = executorService.submit(new Runnable() {
        @Override
        public void run() {
          asyncProfiler.addThread(Thread.currentThread());
          asyncProfiler.setContext(id, 42);
          try {
            Thread.sleep(2);
          } catch(InterruptedException e) {
            Thread.currentThread().interrupt();
          }
          asyncProfiler.removeThread(Thread.currentThread());
        }
      });
    }
    for (Future<?> future : futures) {
      future.get();
    }
  }
}
