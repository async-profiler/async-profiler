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

    private static JvmType detectJvm() {
        //example jvmdir: /usr/lib/jvm/java-17-amazon-corretto.x86_64/bin
        String jvmDirectory = System.getProperty("java.home");
        Path path = Paths.get(jvmDirectory);
        Path parentPath = path.getParent();

        File directory = new File(parentPath.toString() + "/Home/lib/default"); //j9
        File[] files = directory.listFiles();
        String regexPattern = "libj9thr\\d+\\." + libExt;
        Pattern pattern = Pattern.compile(regexPattern);

        if (files != null) {
            for (File file : files) {
                if (file.isFile()) {
                    String fileName = file.getName();
                    Matcher matcher = pattern.matcher(fileName);
                    if (matcher.matches()) {
                        return JvmType.J9;
                    }
                }
            }
        }

        Path etcPath = parentPath.resolve("etc"); //zing
        File zing = new File(etcPath.toString(), "zing");
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
        } else {
            throw new RuntimeException("Unable to detect the OS used by the host.\nOS may be unsupported.");
        }
    }

    public static boolean enabled(Test test) {
        OsType[] os = test.os();
        JvmType[] jvm = test.jvm();
        ArchType[] arch = test.arch();
        
        if (!test.enabled()){
            return false;
        }
    
        if (os.length > 0 && !Arrays.asList(os).contains(currentOs)) {
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

    public static void run(Method m) throws Exception{
        System.setProperty("java.util.logging.ConsoleHandler.formatter", "java.util.logging.SimpleFormatter");
        System.setProperty("java.util.logging.SimpleFormatter.format", "%4$s: %5$s%6$s%n");

        for (Test test : m.getAnnotationsByType(Test.class)) {
            if (!enabled(test)) {
                log.log(Level.FINE, "Skipped " + m.getDeclaringClass().getName() + '.' + m.getName());
                continue;
            }

            log.log(Level.INFO, "Running " + m.getDeclaringClass().getName() + '.' + m.getName() + "...");
            try (final TestProcess p = new TestProcess(test, libExt)){
                Object holder = (m.getModifiers() & Modifier.STATIC) == 0 ? m.getDeclaringClass().newInstance() : null;
                m.invoke(holder, p);
                log.info("OK");
            } catch (CommandFail e) {
                log.log(Level.WARNING, "Command call failed. Stderr: " + e.getStderr().toString(), e.getCause());
            } catch (Throwable e) {
                log.log(Level.WARNING, "Test failed", e.getCause());
            }
        }
    }

    public static void run(Class<?> cls) throws Exception{
        for (Method m : cls.getMethods()) {
            run(m);
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length == 0) {
            System.out.println("Usage: java " + Runner.class.getName() + " TestName ...");
            System.exit(1);
        }

        for (String testName : args) {
            if (testName.indexOf('.') < 0 && Character.isLowerCase(testName.charAt(0))) {
                // Convert package name to class name
                testName = "test." + testName + "." + Character.toUpperCase(testName.charAt(0)) + testName.substring(1) + "Tests";
            }
            run(Class.forName(testName));
        }
    }
}
