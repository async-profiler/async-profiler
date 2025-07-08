/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.lock;

import java.util.concurrent.Semaphore;
import java.util.function.Supplier;

public class SimpleLockTest {

    private static final Semaphore semaphore = new Semaphore(1);

    private static synchronized void syncLock() throws InterruptedException {
        Thread.sleep(100);
    }

    private static void semLock() throws InterruptedException {
        semaphore.acquire();
        Thread.sleep(100);
        semaphore.release();
    }

    public static void main(String[] args) throws InterruptedException {
        if (args.length < 1) {
            System.out.println("Please input lock type: sync or sem");
            System.exit(1);
        }

        Supplier<Runnable> runnableSupplier = () -> () -> {
            try {
                if (args[0].equals("sync")) {
                    syncLock();
                } else if (args[0].equals("sem")) {
                    semLock();
                }
            } catch (InterruptedException e) {
                throw new RuntimeException(e);
            }
        };

        Thread thread1 = new Thread(runnableSupplier.get());
        Thread thread2 = new Thread(runnableSupplier.get());

        thread1.start();
        thread2.start();

        thread1.join();
        thread2.join();
    }
}
