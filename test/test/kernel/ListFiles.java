package test.kernel;

import java.io.File;

public class ListFiles {
    private static volatile int value;

    private static void listFiles() {
        for (String s : new File("/tmp").list()) {
            value += s.hashCode();
        }
    }

<<<<<<< HEAD
    public static void main(String[] args) {
=======
    public static void main(String[] args){
>>>>>>> 2a8a0c7 (add back testing framework and update tests to fix most assertion failus)
        while (true) {
            listFiles();
        }
    }
}
