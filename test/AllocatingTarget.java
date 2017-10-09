import java.util.concurrent.ThreadLocalRandom;

public class AllocatingTarget {
    public static volatile Object sink;

    public static void main(String[] args) {
        while (true) {
            allocate();
        }
    }

    private static void allocate() {
        if (ThreadLocalRandom.current().nextBoolean()) {
            sink = new int[128 * 1000];
        } else {
            sink = new Integer[128 * 1000];
        }
    }
}
