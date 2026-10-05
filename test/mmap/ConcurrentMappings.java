/* Copyright The async-profiler authors. SPDX-License-Identifier: Apache-2.0 */
package probe;
import com.sun.jna.*;
import java.io.*;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentMappings {
    public static void main(String[] args) throws Exception {
        Demo.LibC c = Native.load(Platform.C_LIBRARY_NAME, Demo.LibC.class);
        long page = c.getpagesize();
        int flags = 2 | (Platform.isMac() ? 0x1000 : 0x20);
        long warm = c.mmap(0, page, 3, flags, -1, 0);
        if (warm == -1 || c.munmap(warm, page) != 0) throw new AssertionError();
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in));
        System.out.println("READY");
        if (!"go".equals(in.readLine())) throw new AssertionError();
        AtomicReference<Throwable> failure = new AtomicReference<>();
        Thread[] threads = new Thread[4];
        for (int t = 0; t < threads.length; t++) {
            threads[t] = new Thread(() -> {
                try {
                    for (int n = 0; n < 100; n++) {
                        long p = c.mmap(0, page * 3, 3, flags, -1, 0);
                        if (p == -1 || c.munmap(p + page, page) != 0 ||
                            c.munmap(p, page) != 0 || c.munmap(p + 2 * page, page) != 0)
                            throw new AssertionError("mapping failed");
                    }
                } catch (Throwable e) { failure.set(e); }
            });
            threads[t].start();
        }
        for (Thread t : threads) t.join();
        if (failure.get() != null) throw new AssertionError(failure.get());
        long retained = c.mmap(0, page, 3, flags, -1, 0);
        if (retained == -1) throw new AssertionError();
        System.out.println("DONE retained=0x" + Long.toHexString(retained) + " bytes=" + page);
        if (!"cleanup".equals(in.readLine()) || c.munmap(retained, page) != 0) throw new AssertionError();
        System.out.println("CLEANED");
    }
}
