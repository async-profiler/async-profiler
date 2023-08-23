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

public class Runner {
    private static final Logger log = Logger.getLogger(Runner.class.getName());

    private static final OsType currentOs = detectOs();
    private static String libExt;
    private static final CpuArcType currentCpuArc = detectCpuArc();
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
        String jvmDirectory = System.getProperty("java.home").toLowerCase();
        Path path = Paths.get(jvmDirectory);
        Path parentPath = path.getParent();

        Path libPath = parentPath.resolve("lib");
        File libj9thr = new File(libPath.toString(), "libj9thr" + libExt); //j9

        Path etcPath = parentPath.resolve("etc");
        File zing = new File(etcPath.toString(), "zing"); //zing

        // Check if the file exists
        if (libj9thr.exists()) {
            return JvmType.J9;
        } else if (zing.exists()) {
            return JvmType.ZING;
        } else{
            return JvmType.HOTSPOT;
        }
    }

    private static CpuArcType detectCpuArc() {
        String architecture = System.getProperty("os.arch");

        System.out.println("CPU Architecture: " + architecture);

        if (architecture.contains("x86_64") || architecture.contains("amd64")) {
            return CpuArcType.X64;
//        } else if (architecture.contains("x86") || architecture.contains("i386")) {
//            return CpuArcType.X32;
        } else if (architecture.contains("aarch64")) {
            return CpuArcType.ARM64;
        } else if (architecture.contains("arm")) {
            return CpuArcType.ARM32;
        } else if (architecture.contains("ppc64le")) {
            return CpuArcType.PPC64LE;
        } else {
            return CpuArcType.X64;
        }
    }

    public static boolean enabled(Test test) {
        OsType[] os = test.os();
        JvmType[] jvm = test.jvm();
        CpuArcType[] cpuArc = test.cpuArc();
        
        if (!test.enabled()){
            return false;
        }
    
        if (os.length > 0 && !Arrays.asList(os).contains(currentOs)) {
            return false;
        }

        if (jvm.length > 0 && !Arrays.asList(jvm).contains(currentJvm)) {
            return false;
        }

        if (cpuArc.length > 0 && !Arrays.asList(cpuArc).contains(currentCpuArc)) {
            return false;
        }

        return true;
    }

    public static void run(Method m) {
        System.setProperty("java.util.logging.ConsoleHandler.formatter", "java.util.logging.SimpleFormatter");
        System.setProperty("java.util.logging.SimpleFormatter.format", "%4$s: %5$s%6$s%n");

        for (Test test : m.getAnnotationsByType(Test.class)) {
            if (!enabled(test)) {
                log.log(Level.FINE, "Skipped " + m.getDeclaringClass().getName() + '.' + m.getName());
                continue;
            }

            log.log(Level.INFO, "Running " + m.getDeclaringClass().getName() + '.' + m.getName() + "...");
            try (TestProcess p = new TestProcess(test, libExt)) {
                Object holder = (m.getModifiers() & Modifier.STATIC) == 0 ? m.getDeclaringClass().newInstance() : null;
                m.invoke(holder, p);
                log.info("OK");
            //} catch (InvocationTargetException e) {
            //    System.out.println("reached here");
            //    log.log(Level.WARNING, "Test failed", e.getCause());
            } catch (Throwable e) {
                log.log(Level.WARNING, "Test failed", e);
            }
        }
    }

    public static void run(Class<?> cls) {
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
