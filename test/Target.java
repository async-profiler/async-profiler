import java.util.Scanner;
import java.io.File;

class Target {
  private static volatile int value;

  private static void method1() {
    for (int i = 0; i < 1000000; ++i)
      ++value;
  }

  private static void method2() {
    for (int i = 0; i < 1000000; ++i)
      ++value;
  }

  private static void method3() throws Exception {
    for (int i = 0; i < 1000; ++i) {
      new Scanner(new File("/proc/cpuinfo")).useDelimiter("\\Z").next();
    }
  }

  public static void main(String[] args) throws Exception {
    while (true) {
      method1();
      method2();
      method3();
    }
  }
}
