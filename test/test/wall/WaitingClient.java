/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.wall;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CopyOnWriteArrayList;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.function.Supplier;

class WaitingClient {
    private static final ExecutorService executor = Executors.newCachedThreadPool();
    public static void main(String[] args) throws Exception {
        for (int i = 0, id = 1; i < 5000; i++, id += 3) {
            method1(id);
        }
    }

    public static void method1(int id) throws ExecutionException, InterruptedException {
        method1Impl(id);
    }

    public static void method1Impl(int id) throws ExecutionException, InterruptedException {
        sleep(10);
        Object monitor = new Object();
        Future<?> wait = executor.submit(() -> method3(id, monitor));
        method2(id, monitor);
        synchronized (monitor) {
            monitor.wait(10);
        }
        wait.get();
    }

    public static void method2(long id, Object monitor) {
        synchronized (monitor) {
            method2Impl();
            monitor.notify();
        }
    }

    public static void method2Impl() {
        sleep(10);
    }

    public static void method3(long id, Object monitor) {
        synchronized (monitor) {
            method3Impl();
            monitor.notify();
        }
    }

    public static void method3Impl() {
        sleep(10);
    }

    private static void sleep(long millis) {
        try {
            Thread.sleep(millis);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
