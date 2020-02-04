import java.lang.management.ClassLoadingMXBean;
import java.lang.management.ManagementFactory;

class LoadLibraryTest {

    public static void main(String[] args) throws Exception {
        for (int i = 0; i < 200; i++) {
            Thread.sleep(10);
        }

        // Late load of libmanagement.so
        ClassLoadingMXBean bean = ManagementFactory.getClassLoadingMXBean();

        long n = 0;
        while (n >= 0) {
            n += bean.getLoadedClassCount();
            n += bean.getTotalLoadedClassCount();
            n += bean.getUnloadedClassCount();
        }
    }
}
