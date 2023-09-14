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
    private static final int currentJvmVersion = detectJvmVersion();
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
        File[] files = directory.listFiles();
        if (files != null) {
            for (File file : files) {
                if (pattern.matcher(file.getName()).matches()) {
                    return true;
                }
            }
        }
        return false;
    }

    private static int detectJvmVersion() throws RuntimeException { // Not sure if this works for zing 
            String prop = System.getProperty("java.vm.version");
            int jvmVersion;
            
            if (prop != null) {
                if (prop.startsWith("25.") && prop.charAt(3) > '0') {
                    jvmVersion = 8;
                } else if (prop.startsWith("24.") && prop.charAt(3) > '0') {
                    jvmVersion = 7;
                } else if (prop.startsWith("20.") && prop.charAt(3) > '0') {
                    jvmVersion = 6;
                } else {
                    try {
                        jvmVersion = Integer.parseInt(prop);
                    } catch (NumberFormatException e) {
                        jvmVersion = 9;
                    }
                }
            } else {
                throw new RuntimeException();
            }
            return jvmVersion;
    }

    private static JvmType detectJvm() throws RuntimeException{
        // This is here in case user installs java in weird place. Example parentDirectory: /usr/lib/jvm
        File parentDirectory = new File(System.getProperty("java.home")).getParentFile();

        // Workaround for java.home including /jre in JDK 8
        if (currentJvmVersion == 8) {
            parentDirectory = parentDirectory.getParentFile();
        }
    
        // J9 Check
        File directory;
        if (currentOs == OsType.MACOS) {
            // Example lib default dir: /Library/Java/JavaVirtualMachines/amazon-corretto-21.jdk/Contents/Home/lib/default
            directory = new File(parentDirectory, "Home/lib/default");
        } else {
            // Example lib default dir: /usr/lib/jvm/j9jdk/jre/lib/(x64|amd64|i486|..)/default
            String archDirectory;
            switch (currentArch) {
                case X64:
                    archDirectory = "amd64";
                    break;
                case X86:
                    archDirectory = "i386";
                    break;
                case ARM64:
                    archDirectory = "aarch64";
                    break;
                case ARM32:
                    archDirectory = "arm";
                    break;
                case PPC64LE:
                    archDirectory = "ppc64le";
                    break;
                default: // This should never be reached but is here to quiet the compiler
                    throw new RuntimeException();
            }
            directory = new File(parentDirectory, "jre/lib" + "/" + archDirectory);
        }

        String libj9String = "libj9thr\\d+\\." + libExt;
        Pattern libj9Pattern = Pattern.compile(libj9String);
        File[] files = directory.listFiles();
        if (files != null) {
            for (File file : files) {
                if (libj9Pattern.matcher(file.getName()).matches()) {
                    return JvmType.J9;
                }
            }
        }
    
        // Zing check
        File etcDirectory = new File(parentDirectory, "etc"); // zing
        File zing = new File(etcDirectory, "zing");
        if (zing.exists()) {
            return JvmType.ZING;
        }
    
        // Otherwise it's some variation of hotspot
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
        } else if (architecture.matches(".*86$") || architecture.contains("i386") || architecture.contains("i486") || architecture.contains("i586") || architecture.contains("i686")) {
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

    public static void run(Method m, Boolean retainLog) throws Exception{
        for (Test test : m.getAnnotationsByType(Test.class)) {
            if (!enabled(test)) {
                log.log(Level.FINE, "Skipped " + m.getDeclaringClass().getName() + '.' + m.getName());
                continue;
            }
            String testName = null;
            if (retainLog) {
                testName = m.getDeclaringClass().getName() + '.' + m.getName();
            }

            log.log(Level.INFO, "Running " + m.getDeclaringClass().getName() + '.' + m.getName() + "...");

            try (final TestProcess p = new TestProcess(test, libExt, testName);) {
                Object holder = (m.getModifiers() & Modifier.STATIC) == 0 ? m.getDeclaringClass().newInstance() : null;
                m.invoke(holder, p);
                log.info("OK");
            } catch (Throwable e) {
                System.out.println(e.getCause());
                log.log(Level.WARNING, "Test failed " + e.getCause().getMessage(), e.getCause().getStackTrace());
            }
        }
    }

    public static void run(Class<?> cls, Boolean retainLog) throws Exception{
        for (Method m : cls.getMethods()) {
            run(m, retainLog);
        }
    }

    private static boolean configureLogging(String[] args) {
        if (System.getProperty("java.util.logging.ConsoleHandler.formatter") == null) {
            System.setProperty("java.util.logging.ConsoleHandler.formatter", "java.util.logging.SimpleFormatter");
        }
        if (System.getProperty("java.util.logging.SimpleFormatter.format") == null) {
            System.setProperty("java.util.logging.SimpleFormatter.format", "%4$s: %5$s%6$s%n");
        }

        String logLevelProperty = System.getProperty("logLevel");
        if (logLevelProperty != null) {
            Level logLevel =  Level.parse(logLevelProperty);
            if (logLevel != null){
                Logger.getLogger("").setLevel(logLevel);
            } else {
                Logger.getLogger("").setLevel(Level.INFO);
            }
        } else {
            Logger.getLogger("").setLevel(Level.INFO);
        }

        String retainProfilesProperty = System.getProperty("retainProfiles");
        if (retainProfilesProperty != null) {
            if (retainProfilesProperty.equals("true")) {
                return true;
            } else if (retainProfilesProperty.equals("false")) {
                return false;
            } else {
                return true;
            }
        } else {
            return true;
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.out.println("Usage: java " + Runner.class.getName() + " TestName ...");
            System.exit(1);
        }

        Boolean retainLogs = configureLogging(args);

        for (int i = 0; i < args.length; i++) {
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
