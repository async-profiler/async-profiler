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
import java.io.UncheckedIOException;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.StringTokenizer;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;

import one.jfr.JfrReader;

public class TestProcess implements AutoCloseable {
    private static final Logger log = Logger.getLogger(TestProcess.class.getName());

    private static final Pattern filePattern = Pattern.compile("(%[a-z]+)(\\.[a-z]+)?");

    private static final MethodHandle pid = getPidHandle();

    private static MethodHandle getPidHandle() {
        // JDK 9+
        try {
            return MethodHandles.publicLookup().findVirtual(Process.class, "pid", MethodType.methodType(long.class));
        } catch (ReflectiveOperationException e) {
            // fallback
        }

        // JDK 8
        try {
            Field f = Class.forName("java.lang.UNIXProcess").getDeclaredField("pid");
            f.setAccessible(true);
            return MethodHandles.lookup().unreflectGetter(f).asType(MethodType.methodType(long.class, Process.class));
        } catch (ReflectiveOperationException e) {
            throw new IllegalStateException("Unsupported API", e);
        }
    }

    private final Process p;
    private final Map<String, File> tmpFiles = new HashMap<>();
    private final int timeout = 30;

    public TestProcess(Test test, String libExt) throws Exception {
        List<String> cmd = new ArrayList<>();
        cmd.add(System.getProperty("java.home") + "/bin/java");
        cmd.add("-cp");
        cmd.add(System.getProperty("java.class.path"));
        if (test.debugNonSafepoints()) {
            cmd.add("-XX:+UnlockDiagnosticVMOptions");
            cmd.add("-XX:+DebugNonSafepoints");
        }
        addArgs(cmd, test.jvmArgs());
        if (!test.agentArgs().isEmpty()) {
            cmd.add("-agentpath:build/lib/libasyncProfiler." + libExt + "=" + substituteFiles(test.agentArgs()));
        }
        cmd.add(test.mainClass().getName());
        addArgs(cmd, test.args());
        log.log(Level.FINE, "Running " + cmd);
        ProcessBuilder pb = new ProcessBuilder(cmd).inheritIO();
        if (test.output()) {
            pb.redirectOutput(createTempFile("%out", null));
        }
        if (test.error()) {
            pb.redirectError(createTempFile("%err", null));
        }
        this.p = pb.start();
    }

    private void addArgs(List<String> cmd, String args) {
        if (args != null && !args.isEmpty()) {
            args = substituteFiles(args);
            for (StringTokenizer st = new StringTokenizer(args, " "); st.hasMoreTokens(); ) {
                cmd.add(st.nextToken());
            }
        }
    }

    private String substituteFiles(String s) {
        Matcher m = filePattern.matcher(s);
        if (!m.find()) {
            return s;
        }

        StringBuffer sb = new StringBuffer();
        do {
            File f = createTempFile(m.group(1), m.group(2));
            m.appendReplacement(sb, f.toString());
        } while (m.find());

        m.appendTail(sb);
        return sb.toString();
    }

    private File createTempFile(String fileId, String suffix) {
        try {
            File f = File.createTempFile("ap-" + fileId.substring(1), suffix);
            tmpFiles.put(fileId, f);
            return f;
        } catch (IOException e) {
            throw new UncheckedIOException(e);
        }
    }

    private void clearTempFiles() {
        for (File file : tmpFiles.values()) {
            file.delete();
        }
    }

    @Override
    public void close() {
        p.destroy();
        
        try {
            waitForExit(p, 20);
        } catch (TimeoutException e) {
            log.log(Level.WARNING, "Failed to terminate child process", e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } finally {
            clearTempFiles();
        }
    }

    public long pid() {
        try {
            return (long) pid.invokeExact(p);
        } catch (Throwable e) {
            throw new IllegalStateException("Unsupported API", e);
        }
    }

    public Output waitForExit(String fileId) throws TimeoutException, InterruptedException {
        waitForExit(p, timeout);
        return readFile(fileId);
    }

    private void waitForExit(Process p, int seconds) throws TimeoutException, InterruptedException {
        if (!p.waitFor(seconds, TimeUnit.SECONDS)) {
            p.destroyForcibly();
            throw new TimeoutException("Child process has not exited");
        }
    }

    public Output profile(String args) throws IOException, TimeoutException, InterruptedException, CommandFail {
        return profile(args, false);
    }

    public Output profile(String args, boolean sudo) throws IOException, TimeoutException, InterruptedException, CommandFail {
        // Give JVM process some time to initialize
        Thread.sleep(2000);

        List<String> cmd = new ArrayList<>();
        if (sudo) {
            cmd.add("/usr/bin/sudo");
        }
        cmd.add("build/bin/asprof");
        addArgs(cmd, args);
        cmd.add(Long.toString(pid()));
        log.log(Level.FINE, "Profiling " + cmd);
        
        Process p = new ProcessBuilder(cmd)
                .redirectOutput(createTempFile("%pout", null))
                .redirectError(createTempFile("%perr", null))
                .start();

        waitForExit(p,10);
        int exitCode = p.waitFor();
        if (exitCode != 0) {
            throw new CommandFail("Profiling call failed " + readFile("%pout").toString() + readFile("%perr").toString(), readFile("%perr"));
        }

        return readFile("%pout");
    }

    public Output readFile(String fileId) {
        File f = tmpFiles.get(fileId);
        try (Stream<String> stream = Files.lines(f.toPath())) {
            return new Output(stream.toArray(String[]::new));
        } catch (IOException e) {
            return new Output(new String[0]);
        }
    }

}
