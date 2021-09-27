import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ThreadLocalRandom;

public class MemLeakTarget implements Runnable {
    public static volatile List<Object> sink = new ArrayList<>();

    public static void main(String[] args) {
        new Thread(new MemLeakTarget(), "AllocThread-1").start();
        new Thread(new MemLeakTarget(), "AllocThread-2").start();
    }

    @Override
    public void run() {
        ThreadLocalRandom random = ThreadLocalRandom.current();
        while (true) {
            allocate(random, 16);
        }
    }

    private static void allocate(ThreadLocalRandom random, int depth) {
        if (depth == 0 || random.nextInt(depth) != 0) {
            allocate(random, depth - 1);
            return;
        }

        Object obj;
        if (random.nextBoolean()) {
            obj = new int[random.nextInt(64, 192) * 1000];
        } else {
            obj = new Integer[random.nextInt(64, 192) * 1000];
        }

        if (random.nextInt(1000) == 0) {
            sink.add(obj);
        }
    }
}
