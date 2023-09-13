/*
 * Copyright 2021 Andrei Pangin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package one.profiler.test;

import java.io.File;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Arrays;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Pattern;
import java.util.regex.Matcher;

public class Runner {
    private static final Logger log = Logger.getLogger(Runner.class.getName());

    private static final OsType currentOs = detectOs();
    private static String libExt;
    private static final ArchType currentArch = detectArch();
    private static final int currentJdkVersion = detectJdkVersion();
    private static final JvmType currentJvm = detectJvm();

    private static OsType detectOs() {
        String osName = System.getProperty("os.name").toLowerCase();
        if (osName.contains("linux")) {
            libExt = "so";
            return OsType.LINUX;
        } else if (osName.contains("mac")) {
            libExt = "dylib";
            return OsType.MACOS;
        } else if (osName.contains("windows")) {
            libExt = "dll";
            return OsType.WINDOWS;
        }
        throw new IllegalStateException("Unknown OS type");
    }

    private static boolean searchForMatch(File directory, Pattern pattern) {
        return searchForMatch(directory, pattern, false);
    }
    private static boolean searchForMatch(File directory, Pattern pattern, Boolean passedDefault) {
        File[] files = directory.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    if (file.getName().equals("default")) {
                        passedDefault = true;
                    }
                    searchForMatch(file, pattern, passedDefault); // Recursively search subdirectories
                } else {
                    // Don't bother checking the regex if we haven't even passed default dir
                    if (passedDefault && pattern.matcher(file.getName()).matches()) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    private static int detectJdkVersion() {
        String javaVersion = System.getProperty("java.version");
        String[] versionParts = javaVersion.split("\\."); 
        int majorVersion = Integer.parseInt(versionParts[0]);
        return majorVersion;
    }

    private static JvmType detectJvm() {
        // Example jvmdir: /usr/lib/jvm/java-17-amazon-corretto.x86_64/bin
        String jvmDirectory = System.getProperty("java.home");
        File jvmDirectoryFile = new File(jvmDirectory);
        File parentDirectory = jvmDirectoryFile.getParentFile();

        // Workaround for java.home including /jre in JDK 8
        if (currentJdkVersion == 8 && currentJvm == JvmType.HOTSPOT) {
            parentDirectory = parentDirectory.getParentFile();
        }
    
        String libj9String = "libj9thr\\d+\\." + libExt;
        Pattern libj9Pattern = Pattern.compile(libj9String);
        if (currentOs == OsType.MACOS) {
            // Example lib default dir: /Library/Java/JavaVirtualMachines/amazon-corretto-21.jdk/Contents/Home/lib/default
            File directory = new File(parentDirectory, "Home/lib/default");
            if (searchForMatch(directory, libj9Pattern, true)) {
                return JvmType.J9;
            }
        } else {
            // Example lib default dir: /usr/lib/jvm/j9jdk/jre/lib/(x64|amd64|i486|..)/default
            // too many architecture variations so recursive search
            File directory = new File(parentDirectory, "jre/lib/arch");
            if (searchForMatch(directory, libj9Pattern)) {
                return JvmType.J9;
            }
        }
    
        File etcDirectory = new File(parentDirectory, "etc"); // zing
        File zing = new File(etcDirectory, "zing");
        if (zing.exists()) {
            return JvmType.ZING;
        }
    
        return JvmType.HOTSPOT;
    }
    
    private static ArchType detectArch() throws RuntimeException{
        String architecture = System.getProperty("os.arch");

        if (architecture.contains("x86_64") || architecture.contains("amd64")) {
            return ArchType.X64;
        } else if (architecture.contains("aarch64")) {
            return ArchType.ARM64;
        } else if (architecture.contains("arm")) {
            return ArchType.ARM32;
        } else if (architecture.contains("ppc64le")) {
            return ArchType.PPC64LE;
        } else if (architecture.contains("x86") || architecture.contains("i386") || architecture.contains("i486") || architecture.contains("i586") || architecture.contains("i686")) {
            return ArchType.X86;
        } else {
            throw new RuntimeException("Unable to detect the OS used by the host.\nOS may be unsupported.");
        }
    }

    public static boolean enabled(Test test) {
        OsType[] os = test.os();
        int[] jdkVersion = test.jdkVersion();
        JvmType[] jvm = test.jvm();
        ArchType[] arch = test.arch();
        
        if (!test.enabled()) {
            return false;
        }
    
        if (os.length > 0 && !Arrays.asList(os).contains(currentOs)) {
            return false;
        }

        if (jdkVersion.length > 0 && !Arrays.asList(jdkVersion).contains(currentJdkVersion)) {
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

    public static void run(Method m, Boolean retainLog) throws Exception{
        for (Test test : m.getAnnotationsByType(Test.class)) {
            if (!enabled(test)) {
                log.log(Level.FINE, "Skipped " + m.getDeclaringClass().getName() + '.' + m.getName());
                continue;
            }

            log.log(Level.INFO, "Running " + m.getDeclaringClass().getName() + '.' + m.getName() + "...");
            final TestProcess p;
            
            try {
                p = new TestProcess(test, libExt, retainLog);
            } catch (Throwable e) {
                log.log(Level.WARNING, e.getMessage());
                return;
            }

            try {
                Object holder = (m.getModifiers() & Modifier.STATIC) == 0 ? m.getDeclaringClass().newInstance() : null;
                m.invoke(holder, p);
                log.info("OK");
            } catch (Throwable e) {
                log.log(Level.WARNING, "Test failed " + e.getMessage(), e.getStackTrace());
            } finally {
                try {
                    p.close();
                } catch (Throwable e) {
                    log.log(Level.SEVERE, "Testing framework test could not close properly. " + e.getMessage(), e.getStackTrace());
                }
            }
        }
    }

    public static void run(Class<?> cls, Boolean retainLog) throws Exception{
        for (Method m : cls.getMethods()) {
            run(m, retainLog);
        }
    }

    public static void main(String[] args) throws Exception {
        String propertyName = "java.util.logging.ConsoleHandler.formatter";
        String propertyValue = System.getProperty(propertyName);

        if (propertyValue == null) {
            System.setProperty("java.util.logging.ConsoleHandler.formatter", "java.util.logging.SimpleFormatter");
        }
        propertyName = "java.util.logging.SimpleFormatter.format";
        propertyValue = System.getProperty(propertyName);
        if (propertyValue == null) {
            System.setProperty("java.util.logging.SimpleFormatter.format", "%4$s: %5$s%6$s%n");
        }

        if (args.length == 0) {
            System.out.println("Usage: java " + Runner.class.getName() + " TestName ...");
            System.exit(1);
        }

        Logger rootLogger = Logger.getLogger("");
        String logLevel =  args[0];
        if (logLevel.equals("SEVERE")){
            rootLogger.setLevel(Level.SEVERE);
        } else if (logLevel.equals("WARNING")){
            rootLogger.setLevel(Level.WARNING);
        } else if (logLevel.equals("INFO")){
            rootLogger.setLevel(Level.INFO);
        } else if (logLevel.equals("CONFIG")){
            rootLogger.setLevel(Level.CONFIG);
        } else if (logLevel.equals("FINE")){
            rootLogger.setLevel(Level.FINE);
        } else if (logLevel.equals("FINER")){
            rootLogger.setLevel(Level.FINER);
        } else if (logLevel.equals("FINEST")){
            rootLogger.setLevel(Level.FINEST);
        } else if (logLevel.equals("OFF")){
            rootLogger.setLevel(Level.OFF);
        } else if (logLevel.equals("ALL")){
            rootLogger.setLevel(Level.ALL);
        } else {
            log.log(Level.WARNING, "Invalid logging level enum. Available are: SEVERE,WARNING,INFO,CONFIG,FINE,FINER,FINEST,OFF,ALL");
            log.log(Level.WARNING, "Got: " + args[0]);
            for (String name : args) {
                log.log(Level.WARNING, name);
            }
            rootLogger.setLevel(Level.INFO);
        }
        Boolean retainLogs;
        if (args[1].equals("true")){
            retainLogs = true;
        } else if (args[1].equals("false")) {
            retainLogs = false;
        } else {
            log.log(Level.WARNING, "retainLogS option is malformed. Got: " + args[1]);
            retainLogs = false;
        }

        for (int i = 2; i < args.length; i++) {
            String testName = args[i];
            if (args[i].indexOf('.') < 0 && Character.isLowerCase(args[i].charAt(0))) {
                // Convert package name to class name
                testName = "test." + testName + "." + Character.toUpperCase(testName.charAt(0)) + testName.substring(1) + "Tests";
            }
            run(Class.forName(testName), retainLogs);
        }
        if (retainLogs) {
            log.log(Level.INFO, "Test/profile stderr stdout and profile are available in build/test/logs where applicable.");
        }
    }
}
