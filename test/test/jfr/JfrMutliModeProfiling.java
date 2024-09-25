package test.jfr;

import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.TimeUnit;
import java.util.stream.IntStream;

/**
 * Process to simulate lock contention and allocate objects.
 */
public class JfrMutliModeProfiling {
    private static final Object lock = new Object();

    private static Object sink;
    private static int count = 0;

    public static void main(String[] args) throws InterruptedException {
        ExecutorService executor = Executors.newFixedThreadPool(2);
        IntStream.range(0, 1_000_000_000).forEach(i -> executor.submit(JfrMutliModeProfiling::increment));
        allocate();
        stop(executor);
        System.out.println(count);
    }

    private static void increment() {
        synchronized (lock) {
            count += 1;
        }
    }

    private static void stop(ExecutorService executor) throws InterruptedException {
        executor.shutdown();
        executor.awaitTermination(10, TimeUnit.SECONDS);
        executor.shutdownNow();
    }

    private static void allocate() {
        long start = System.currentTimeMillis();
        Random random = ThreadLocalRandom.current();
        while (System.currentTimeMillis() - start <= 3000) {
            if (random.nextBoolean()) {
                sink = new int[128 * 1000];
            } else {
                sink = new Integer[128 * 1000];
            }
        }
    }
}
