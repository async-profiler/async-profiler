/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.*;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.Comparator;

public class Runner {
    private static final Logger log = Logger.getLogger(Runner.class.getName());

    private static final Os currentOs = detectOs();
    private static final Arch currentArch = detectArch();
    private static final Jvm currentJvm = detectJvm();
    private static final int currentJvmVersion = detectJvmVersion();

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

        // Otherwise it's some variation of HotSpot
        return Jvm.HOTSPOT;
    }

    private static int detectJvmVersion() {
        String prop = System.getProperty("java.vm.specification.version");
        if (prop.startsWith("1.")) {
            prop = prop.substring(2);
        }
        return Integer.parseInt(prop);
    }

    private static boolean enabled(RunnableTest rt, RunnerDeclaration decl) {
        return rt.test().enabled() && decl.matches(rt.method());
    }

    private static boolean applicable(Test test) {
        Os[] os = test.os();
        Arch[] arch = test.arch();
        Jvm[] jvm = test.jvm();
        int[] jvmVer = test.jvmVer();
        return (os.length == 0 || Arrays.asList(os).contains(currentOs)) &&
                (arch.length == 0 || Arrays.asList(arch).contains(currentArch)) &&
                (jvm.length == 0 || Arrays.asList(jvm).contains(currentJvm)) &&
                (jvmVer.length == 0 || (currentJvmVersion >= jvmVer[0] && currentJvmVersion <= jvmVer[jvmVer.length - 1]));
    }

    private static TestResult run(RunnableTest rt, RunnerDeclaration decl) {
        if (!enabled(rt, decl)) {
            return TestResult.skipDisabled();
        }
        if (!applicable(rt.test())) {
            return TestResult.skipConfigMismatch();
        }

        log.log(Level.INFO, "Running " + rt.testInfo() + "...");

        String testLogDir = logDir.isEmpty() ? null : logDir + '/' + rt.testName();
        try (TestProcess p = new TestProcess(rt.test(), currentOs, testLogDir)) {
            Object holder = (rt.method().getModifiers() & Modifier.STATIC) == 0 ?
                    rt.method().getDeclaringClass().getDeclaredConstructor().newInstance() : null;
            rt.method().invoke(holder, p);
        } catch (InvocationTargetException e) {
            return TestResult.fail(e.getTargetException());
        } catch (Throwable e) {
            return TestResult.fail(e);
        }

        return TestResult.pass();
    }

    private static List<RunnableTest> getRunnableTests(Class<?> cls) {
        List<RunnableTest> rts = new ArrayList<>();
        for (Method m : cls.getMethods()) {
            for (Test t : m.getAnnotationsByType(Test.class)) {
                rts.add(new RunnableTest(m, t));
            }
        }
        return rts;
    }

    private static List<RunnableTest> getRunnableTests(String dir) throws ClassNotFoundException {
        String className = "test." + dir + "." + Character.toUpperCase(dir.charAt(0)) + dir.substring(1) + "Tests";
        return getRunnableTests(Class.forName(className));
    }

    private static List<RunnableTest> getRunnableTests(RunnerDeclaration decl) throws ClassNotFoundException {
        List<RunnableTest> rts = new ArrayList<>();
        File[] files = new File("test/test").listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    rts.addAll(getRunnableTests(file.getName()));
                }
            }
        }
        rts.sort(Comparator.comparing(RunnableTest::testName));
        return rts;
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

    private static void printSummary(EnumMap<TestStatus, Integer> statusCounts, List<String> failedTests, long totalTestDuration, int testCount) {
        int fail = statusCounts.getOrDefault(TestStatus.FAIL, 0);
        if (fail > 0) {
            System.out.println("\nFailed tests:");
            failedTests.forEach(System.out::println);
        }

        int pass = statusCounts.getOrDefault(TestStatus.PASS, 0);
        String totalDuration = String.format("%.3f s", totalTestDuration / 1e9);

        System.out.println("\nTotal test duration: " + totalDuration);
        System.out.println("Results Summary:");
        System.out.printf("PASS: %d (%.1f%%)\n", pass, 100.0 * pass / (pass + fail));
        System.out.println("FAIL: " + fail);
        System.out.println("SKIP (disabled): " + statusCounts.getOrDefault(TestStatus.SKIP_DISABLED, 0));
        System.out.println("SKIP (config mismatch): " + statusCounts.getOrDefault(TestStatus.SKIP_CONFIG_MISMATCH, 0));
        System.out.println("TOTAL: " + testCount);
    }

    private static RunnerDeclaration parseRunnerDeclaration(String[] args) {
        // All available directories.
        List<String> testDirectories = new ArrayList<>();

        // If directories are specified, they are considered an exact match.
        List<String> includeDirs = new ArrayList<>();

        // Glob filters matching "ClassName.methodName".
        List<String> testFilters = new ArrayList<>();

        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            File f = new File("test/test/" + arg);
            if (!f.exists() || !f.isDirectory()) {
                testFilters.add(arg);
            } else {
                includeDirs.add(arg);
            }
        }

        List<String> skipFilters = new ArrayList<>();
        String skipProperty = System.getProperty("skip");
        if (skipProperty != null && !skipProperty.isEmpty()) {
            for (String skip : skipProperty.split(" ")) {
                skipFilters.add(skip);
            }
        }

        log.log(Level.FINE, "Selected directories: " + includeDirs);
        log.log(Level.FINE, "Test Filters: " + testFilters);
        log.log(Level.FINE, "Skip Filters: " + skipFilters);

        return new RunnerDeclaration(includeDirs, testFilters, skipFilters);
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.out.println("Usage: java " + Runner.class.getName() + " TestName ...");
            System.exit(1);
        }

        configureLogging();

        RunnerDeclaration decl = parseRunnerDeclaration(args);
        List<RunnableTest> allTests = getRunnableTests(decl);
        final int testCount = allTests.size();
        int i = 1;
        long totalTestDuration = 0;
        List<String> failedTests = new ArrayList<>();
        EnumMap<TestStatus, Integer> statusCounts = new EnumMap<>(TestStatus.class);
        for (RunnableTest rt : allTests) {
            long start = System.nanoTime();
            TestResult result = run(rt, decl);
            long durationNs = System.nanoTime() - start;

            totalTestDuration += durationNs;
            statusCounts.put(result.status(), statusCounts.getOrDefault(result.status(), 0) + 1);
            if (result.status() == TestStatus.FAIL) {
                failedTests.add(rt.testInfo());
            }

            System.out.printf("%s [%d/%d] %s took %.3f s\n", result.status(), i, testCount, rt.testInfo(), durationNs / 1e9);
            if (result.throwable() != null) {
                result.throwable().printStackTrace(System.out);
            }
            i++;
        }

        printSummary(statusCounts, failedTests, totalTestDuration, testCount);

        if (!logDir.isEmpty()) {
            log.log(Level.INFO, "Test output and profiles are available in " + logDir + " directory");
        }

        if (!failedTests.isEmpty()) {
            throw new RuntimeException("One or more tests failed");
        }
    }
}
