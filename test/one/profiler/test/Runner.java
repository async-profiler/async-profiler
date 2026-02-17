/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Modifier;
import java.util.*;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Executor;
import java.util.concurrent.ThreadPoolExecutor;


public class Runner {
    private static final Logger log = Logger.getLogger(Runner.class.getName());

    private static final Os currentOs = detectOs();
    private static final Arch currentArch = detectArch();
    private static final int currentJvmVersion = detectJvmVersion();
    private static final Jvm currentJvm = detectJvm();

    private static final String logDir = System.getProperty("logDir", "");

    private static Os detectOs() {
        String os = System.getProperty("os.name").toLowerCase();
        if (os.contains("linux")) {
            return Os.LINUX;
        } else if (os.contains("mac")) {
            return Os.MACOS;
        } else if (os.contains("windows")) {
            return Os.WINDOWS;
        }
        throw new IllegalStateException("Unknown OS type");
    }

    private static Arch detectArch() {
        String arch = System.getProperty("os.arch");
        if (arch.contains("x86_64") || arch.contains("amd64")) {
            return Arch.X64;
        } else if (arch.contains("aarch64")) {
            return Arch.ARM64;
        } else if (arch.contains("arm")) {
            return Arch.ARM32;
        } else if (arch.contains("ppc64le")) {
            return Arch.PPC64LE;
        } else if (arch.contains("riscv64")) {
            return Arch.RISCV64;
        } else if (arch.contains("loongarch64")) {
            return Arch.LOONGARCH64;
        } else if (arch.endsWith("86")) {
            return Arch.X86;
        }
        throw new IllegalStateException("Unknown CPU architecture");
    }

    private static Jvm detectJvm() {
        // Example javaHome: /usr/lib/jvm/amazon-corretto-17.0.8.7.1-linux-x64
        File javaHome = new File(System.getProperty("java.home"));

        // Look for OpenJ9-specific file
        File[] files = new File(javaHome, "lib").listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.getName().equals("J9TraceFormat.dat")) {
                    return Jvm.OPENJ9;
                }
            }
        }

        // Strip /jre from JDK 8 path
        if (currentJvmVersion <= 8) {
            javaHome = javaHome.getParentFile();
        }

        // Workaround for Contents/Home on macOS
        if (currentOs == Os.MACOS) {
            javaHome = javaHome.getParentFile();
        }

        // Look for Zing-specific file
        if (new File(javaHome, "etc/zing").exists()) {
            return Jvm.ZING;
        }

        if (!new File(System.getProperty("java.home"), "lib/" + System.mapLibraryName("jvmcicompiler")).exists()) {
            return Jvm.HOTSPOT_C2;
        }

        return Jvm.HOTSPOT;
    }

    private static int detectJvmVersion() {
        String prop = System.getProperty("java.vm.specification.version");
        if (prop.startsWith("1.")) {
            prop = prop.substring(2);
        }
        return Integer.parseInt(prop);
    }

    private static boolean applicable(Test test) {
        Os[] os = test.os();
        Arch[] arch = test.arch();
        Jvm[] jvm = test.jvm();
        int[] jvmVer = test.jvmVer();
        return (os.length == 0 || Arrays.asList(os).contains(currentOs)) &&
                (arch.length == 0 || Arrays.asList(arch).contains(currentArch)) &&
                (jvm.length == 0 || Arrays.asList(jvm).contains(currentJvm) || (currentJvm == Jvm.HOTSPOT_C2 && Arrays.asList(jvm).contains(Jvm.HOTSPOT))) &&
                (jvmVer.length == 0 || (currentJvmVersion >= jvmVer[0] && currentJvmVersion <= jvmVer[jvmVer.length - 1]));
    }

    private static TestResult runTest(RunnableTest rt, TestDeclaration decl) {
        if (!rt.test().enabled() || decl.skips(rt.method())) {
            return TestResult.skipDisabled();
        }
        if (!applicable(rt.test())) {
            return TestResult.skipConfigMismatch();
        }

        log.log(Level.INFO, "Running " + rt.testInfo() + "...");

        String testLogDir = logDir.isEmpty() ? null : logDir + '/' + rt.testName();
        try (TestProcess p = new TestProcess(rt.test(), currentOs, currentJvm, testLogDir)) {
            Object holder = (rt.method().getModifiers() & Modifier.STATIC) == 0 ?
                    rt.method().getDeclaringClass().getDeclaredConstructor().newInstance() : null;
            rt.method().invoke(holder, p);
        } catch (InvocationTargetException e) {
            if (e.getTargetException() instanceof NoClassDefFoundError) {
                return TestResult.skipMissingJar();
            }
            return TestResult.fail(e.getTargetException());
        } catch (Throwable e) {
            return TestResult.fail(e);
        }

        return TestResult.pass();
    }

    private static void configureLogging() {
        if (System.getProperty("java.util.logging.ConsoleHandler.formatter") == null) {
            System.setProperty("java.util.logging.ConsoleHandler.formatter", "java.util.logging.SimpleFormatter");
        }
        if (System.getProperty("java.util.logging.SimpleFormatter.format") == null) {
            System.setProperty("java.util.logging.SimpleFormatter.format", "%4$s: %5$s%6$s%n");
        }

        String logLevelProperty = System.getProperty("logLevel");
        if (logLevelProperty != null && !logLevelProperty.isEmpty()) {
            Level level = Level.parse(logLevelProperty);
            Logger logger = Logger.getLogger("");
            logger.setLevel(level);
            for (Handler handler : logger.getHandlers()) {
                handler.setLevel(level);
            }
        }
    }

    private static void printSummary(Map<TestStatus, Integer> statusCounts, List<String> failedTests, long totalTestDuration, long executionDuration, int testCount) {
        int fail = statusCounts.getOrDefault(TestStatus.FAIL, 0);
        if (fail > 0) {
            System.out.println("\nFailed tests:");
            failedTests.forEach(System.out::println);
        }

        int pass = statusCounts.getOrDefault(TestStatus.PASS, 0);
        String totalDuration = String.format("%.3f s", totalTestDuration / 1e9);
        String actualExecutionDuration = String.format("%.3f s", executionDuration / 1e9);

        System.out.println("\nTotal test duration: " + totalDuration);
        System.out.println("\nActual execution duration: " + actualExecutionDuration);
        System.out.println("Results Summary:");
        System.out.printf("PASS: %d (%.1f%%)\n", pass, 100.0 * pass / (pass + fail));
        System.out.println("FAIL: " + fail);
        System.out.println("SKIP (disabled): " + statusCounts.getOrDefault(TestStatus.SKIP_DISABLED, 0));
        System.out.println("SKIP (config mismatch): " + statusCounts.getOrDefault(TestStatus.SKIP_CONFIG_MISMATCH, 0));
        System.out.println("SKIP (missing JAR): " + statusCounts.getOrDefault(TestStatus.SKIP_MISSING_JAR, 0));
        System.out.println("TOTAL: " + testCount);
    }

    private static void waitForExecutorTermination(ThreadPoolExecutor executor) {
        executor.shutdown(); // Initiate orderly shutdown
        try {
            // Wait indefinitely (or a specific time) for all tasks to finish
            boolean terminated = executor.awaitTermination(5 * 60L, TimeUnit.SECONDS);
            if (terminated) {
                System.out.println("All tasks finished and executor is terminated.");
            } else {
                System.out.println("Timeout occurred before all tasks finished.");
            }
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt(); // Restore the interrupt flag
            System.err.println("Main thread interrupted while waiting for termination.");
        }
    }
    public static void main(String[] args) throws Exception {
        configureLogging();

        TestDeclaration decl = TestDeclaration.parse(args);
        List<RunnableTest> allTests = decl.getRunnableTests();
        final int testCount = allTests.size();
        final int retryCount = Integer.parseInt(System.getProperty("retryCount", "0"));
        final int threadCount = Integer.parseInt(System.getProperty("threadCount", "8"));

        log.log(Level.INFO, "Running with " + threadCount + " test threads.");

        AtomicLong i = new AtomicLong(1);
        AtomicLong totalTestDuration = new AtomicLong();
        List<String> failedTests = Collections.synchronizedList(new ArrayList<>());
        Map<TestStatus, Integer> statusCounts = Collections.synchronizedMap(new EnumMap<>(TestStatus.class));
        BlockingQueue<Runnable> multiThreadWorkQueue = new ArrayBlockingQueue<>(testCount);

        final ThreadPoolExecutor executor = new ThreadPoolExecutor(threadCount, threadCount, 60L, TimeUnit.SECONDS, multiThreadWorkQueue);
        BlockingQueue<Runnable> singleThreadWorkQueue = new ArrayBlockingQueue<>(testCount);

        long startTime = System.nanoTime();
        for (RunnableTest rt : allTests) {
            Runnable task = new Runnable() {
                public void run() {
                    long start = System.nanoTime();
                    TestResult result = runTest(rt, decl);

                    int attempt = 1;
                    while (result.status() == TestStatus.FAIL && attempt <= retryCount) {
                        log.log(Level.WARNING, "Test failed, retrying (attempt " + attempt + "/" + retryCount + ")...");
                        result = runTest(rt, decl);
                        attempt++;
                    }

                    long durationNs = System.nanoTime() - start;

                    totalTestDuration.addAndGet(durationNs);
                    statusCounts.put(result.status(), statusCounts.getOrDefault(result.status(), 0) + 1);
                    if (result.status() == TestStatus.FAIL) {
                        failedTests.add(rt.testInfo());
                    }

                    System.out.printf("tid[%d] %s [%d/%d] %s took %.3f s\n", Thread.currentThread().getId(), result.status(), i.incrementAndGet(), testCount, rt.testInfo(), durationNs / 1e9);
                    if (result.throwable() != null) {
                        result.throwable().printStackTrace(System.out);
                    }
                }
            };
            if (rt.test().runIsolated()) {
                singleThreadWorkQueue.add(task);
            } else {
                executor.execute(task);
            }
        }

        waitForExecutorTermination(executor);

        // Create the Single thread executor and execute all single-threaded tasks
        log.log(Level.INFO, "Starting single threaded tests...");
        final ThreadPoolExecutor singleExecutor = new ThreadPoolExecutor(1, 1, 60L, TimeUnit.SECONDS, singleThreadWorkQueue);
        singleExecutor.prestartAllCoreThreads();
        waitForExecutorTermination(singleExecutor);


        long endTime = System.nanoTime();
        printSummary(statusCounts, failedTests, totalTestDuration.get(), endTime - startTime, testCount);

        if (!logDir.isEmpty()) {
            log.log(Level.INFO, "Test output and profiles are available in " + logDir + " directory");
        }

        if (!failedTests.isEmpty()) {
            throw new RuntimeException("One or more tests failed");
        }
    }
}
