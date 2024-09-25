/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.Logger;

public class Runner {
    private static final Logger log = Logger.getLogger(Runner.class.getName());

    private static final Os currentOs = detectOs();
    private static final Arch currentArch = detectArch();
    private static final Jvm currentJvm = detectJvm();
    private static final int currentJvmVersion = detectJvmVersion();

    private static final Set<String> skipTests = new HashSet<>();
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
        String prop = System.getProperty("java.vm.version");
        if (prop.startsWith("25.") && prop.charAt(3) > '0') {
            return 8;
        } else if (prop.startsWith("24.") && prop.charAt(3) > '0') {
            return 7;
        } else if (prop.startsWith("20.") && prop.charAt(3) > '0') {
            return 6;
        } else {
            return Integer.parseInt(prop.substring(0, prop.indexOf('.')));
        }
    }

    public static boolean enabled(Test test) {
        if (!test.enabled()) {
            return false;
        }

        Os[] os = test.os();
        if (os.length > 0 && !Arrays.asList(os).contains(currentOs)) {
            return false;
        }

        Arch[] arch = test.arch();
        if (arch.length > 0 && !Arrays.asList(arch).contains(currentArch)) {
            return false;
        }

        Jvm[] jvm = test.jvm();
        if (jvm.length > 0 && !Arrays.asList(jvm).contains(currentJvm)) {
            return false;
        }

        int[] jvmVer = test.jvmVer();
        if (jvmVer.length > 0 && (currentJvmVersion < jvmVer[0] || currentJvmVersion > jvmVer[jvmVer.length - 1])) {
            return false;
        }

        return true;
    }

    private static boolean run(Method m) {
        boolean passedAll = true;
        for (Test test : m.getAnnotationsByType(Test.class)) {
            String className = m.getDeclaringClass().getSimpleName();
            String testName = className + '.' + m.getName();

            if (!enabled(test) || skipTests.contains(className.toLowerCase()) || skipTests.contains(m.getName().toLowerCase())) {
                log.log(Level.FINE, "Skipped " + testName);
                continue;
            }

            log.log(Level.INFO, "Running " + testName + "...");

            String testLogDir = logDir.isEmpty() ? null : logDir + '/' + testName;
            try (TestProcess p = new TestProcess(test, currentOs, testLogDir)) {
                Object holder = (m.getModifiers() & Modifier.STATIC) == 0 ?
                        m.getDeclaringClass().getDeclaredConstructor().newInstance() : null;
                m.invoke(holder, p);
                log.info("OK");
            } catch (InvocationTargetException e) {
                log.log(Level.WARNING, testName + " failed", e.getTargetException());
                passedAll = false;
            } catch (Throwable e) {
                log.log(Level.WARNING, testName + " failed", e);
                passedAll = false;
            }
        }
        return passedAll;
    }

    private static boolean run(Class<?> cls) {
        boolean passedAll = true;
        for (Method m : cls.getMethods()) {
            passedAll &= run(m);
        }
        return passedAll;
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

    private static void configureSkipTests() {
        String skipProperty = System.getProperty("skip");
        if (skipProperty != null && !skipProperty.isEmpty()) {
            for (String skip : skipProperty.split(",")) {
                skipTests.add(skip.toLowerCase());
            }
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.out.println("Usage: java " + Runner.class.getName() + " TestName ...");
            System.exit(1);
        }

        configureLogging();
        configureSkipTests();

        boolean passedAll = true;
        for (String arg : args) {
            String testName = arg;
            if (testName.indexOf('.') < 0 && Character.isLowerCase(testName.charAt(0))) {
                // Convert package name to class name
                testName = "test." + testName + "." + Character.toUpperCase(testName.charAt(0)) + testName.substring(1) + "Tests";
            }
            passedAll &= run(Class.forName(testName));
        }

        if (!logDir.isEmpty()) {
            log.log(Level.INFO, "Test output and profiles are available in " + logDir + " directory");
        }

        if (!passedAll) {
            throw new RuntimeException("One or more tests failed");
        }
    }
}
