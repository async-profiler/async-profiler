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
import java.util.Set;
import java.util.HashSet;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Pattern;

public class Runner {
    private static final Logger log = Logger.getLogger(Runner.class.getName());

    private static final Os currentOs = detectOs();
    private static String libExt;
    private static final Arch currentArch = detectArch();
    private static final int currentJvmVersion = detectJvmVersion();
    private static final Jvm currentJvm = detectJvm();

    private static Os detectOs() {
        String osName = System.getProperty("os.name").toLowerCase();
        if (osName.contains("linux")) {
            libExt = "so";
            return Os.LINUX;
        } else if (osName.contains("mac")) {
            libExt = "dylib";
            return Os.MACOS;
        } else if (osName.contains("windows")) {
            libExt = "dll";
            return Os.WINDOWS;
        }
        throw new IllegalStateException("Unknown OS type");
    }

    private static int detectJvmVersion() { // Not sure if this works for zing
        String prop = System.getProperty("java.vm.version");

        if (prop.startsWith("25.") && prop.charAt(3) > '0') {
            return 8;
        } else if (prop.startsWith("24.") && prop.charAt(3) > '0') {
            return 7;
        } else if (prop.startsWith("20.") && prop.charAt(3) > '0') {
            return 6;
        } else {
            String substring = prop.substring(0, prop.indexOf('.'));
            int possibleVersion = Integer.parseInt(substring);
            if (possibleVersion < 9) {
                return 9;
            }
            return possibleVersion;
        }
    }

    private static Jvm detectJvm() throws RuntimeException{
        // Example javaHome: /usr/lib/jvm/amazon-corretto-17.0.8.7.1-linux-x64
        File javaHome = new File(System.getProperty("java.home"));

        // J9 Check which relies on JDK 8 JRE file dir differences
        File libDirectory = new File(javaHome, "lib");
        Pattern j9IndicatorFilePattern = Pattern.compile("J9TraceFormat.dat");
        File[] files = libDirectory.listFiles();
        if (files != null) {
            for (File file : files) {
                if (j9IndicatorFilePattern.matcher(file.getName()).matches()) {
                    return Jvm.J9;
                }
            }
        }

        // Zing check

        // Workaround for java.home including /jre in JDK 8
        if (currentJvmVersion == 8) {
            javaHome = javaHome.getParentFile();
        }

        // Workaround for contents/home on mac
        if (currentOs == Os.MACOS) {
            javaHome = javaHome.getParentFile();
        }
        File etcDirectory = new File(javaHome, "etc");
        File zing = new File(etcDirectory, "zing");
        if (zing.exists()) {
            return Jvm.ZING;
        }

        // Otherwise it's some variation of hotspot
        return Jvm.HOTSPOT;
    }

    private static Arch detectArch() throws RuntimeException{
        String architecture = System.getProperty("os.arch");

        if (architecture.contains("x86_64") || architecture.contains("amd64")) {
            return Arch.X64;
        } else if (architecture.contains("aarch64")) {
            return Arch.ARM64;
        } else if (architecture.contains("arm")) {
            return Arch.ARM32;
        } else if (architecture.contains("ppc64le")) {
            return Arch.PPC64LE;
        } else if (architecture.endsWith("86")) {
            return Arch.X86;
        } else {
            throw new RuntimeException("Unable to detect the OS used by the host.\nOS may be unsupported.");
        }
    }

    public static boolean enabled(Test test) {
        Os[] os = test.os();
        int[] jdkVersion = test.jdkVersion();
        Jvm[] jvm = test.jvm();
        Arch[] arch = test.arch();

        if (!test.enabled()) {
            return false;
        }

        if (os.length > 0 && !Arrays.asList(os).contains(currentOs)) {
            return false;
        }

        if (jdkVersion.length > 0 && !Arrays.asList(jdkVersion).contains(currentJvmVersion)) {
            return false;
        }

        if (jvm.length > 0 && !Arrays.asList(jvm).contains(currentJvm)) {
            return false;
        }

        if (arch.length > 0 && !Arrays.asList(arch).contains(currentArch)) {
            return false;
        }

        return true;
    }

    public static boolean run(Method m, Set skipSet, boolean retainlogs) throws Exception{
        boolean passedAll = true;
        for (Test test : m.getAnnotationsByType(Test.class)) {
            String fullDeclaringClass = m.getDeclaringClass().getName();
            String className = fullDeclaringClass.substring(fullDeclaringClass.lastIndexOf('.') + 1);

            if (!enabled(test) || skipSet.contains(m.getName().toLowerCase()) || skipSet.contains(className.toLowerCase())) {
                log.log(Level.FINE, "Skipped " + m.getDeclaringClass().getName() + '.' + m.getName());
                continue;
            }

            String testName = null;
            if (retainlogs) {
                testName = className + '.' + m.getName();
            }

            log.log(Level.INFO, "Running " + m.getDeclaringClass().getName() + '.' + m.getName() + "...");

            try (final TestProcess p = new TestProcess(test, libExt, testName);) {
                Object holder = (m.getModifiers() & Modifier.STATIC) == 0 ? m.getDeclaringClass().newInstance() : null;
                m.invoke(holder, p);
                log.info("OK"); }
            catch (InvocationTargetException e) {
                log.log(Level.WARNING, "Test failed " + e.getCause().getMessage());
                passedAll = false;
            } catch (Throwable e) {
                log.log(Level.WARNING, "Test failed " + e.getMessage());
                passedAll = false;
            }
        }
        return passedAll;
    }

    public static boolean run(Class<?> cls, Set skipSet, boolean retainlogs) throws Exception{
        boolean passedAll = true;
        for (Method m : cls.getMethods()) {
            passedAll &= run(m, skipSet, retainlogs);
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
        if (logLevelProperty != null){
            Level logLevel = Level.parse(logLevelProperty);
            Logger logger = Logger.getLogger("");
            logger.setLevel(logLevel);
            for (Handler handler : logger.getHandlers()) {
                handler.setLevel(logLevel);
            }

        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.out.println("Usage: java " + Runner.class.getName() + " TestName ...");
            System.exit(1);
        }

        configureLogging();
        boolean retainlogs = !"false".equals(System.getProperty("retainLogs"));

        String[] skips = System.getProperty("skip") != null ? System.getProperty("skip").split(",") : new String[0];
        Set<String> skipSet = new HashSet<>();
        for (String skip : skips) {
            skipSet.add(skip.toLowerCase());
        }

        boolean passedAll = true;
        for (int i = 0; i < args.length; i++) {
            String testName = args[i];
            if (args[i].indexOf('.') < 0 && Character.isLowerCase(args[i].charAt(0))) {
                // Convert package name to class name
                testName = "test." + testName + "." + Character.toUpperCase(testName.charAt(0)) + testName.substring(1) + "Tests";
            }
            boolean success = run(Class.forName(testName), skipSet, retainlogs);
            if (!success) {
                passedAll = false;
            }
        }
        if (retainlogs) {
            log.log(Level.INFO, "Test/profile stderr stdout and profile are available in build/test/logs where applicable.");
        }
        if (!passedAll) {
            throw new RuntimeException("Failed one or more tests.");
        }
    }
}
