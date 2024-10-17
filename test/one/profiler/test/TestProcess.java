/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.io.*;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.*;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.stream.Stream;

public class TestProcess implements Closeable {
    private static final Logger log = Logger.getLogger(TestProcess.class.getName());

    public static final String STDOUT = "%out";
    public static final String STDERR = "%err";
    public static final String PROFOUT = "%pout";
    public static final String PROFERR = "%perr";

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

    private final Test test;
    private final Os currentOs;
    private final String logDir;
    private final String[] inputs;
    private final Process p;
    private final Map<String, File> tmpFiles = new HashMap<>();
    private final int timeout = 30;

    public TestProcess(Test test, Os currentOs, String logDir) throws Exception {
        this.test = test;
        this.currentOs = currentOs;
        this.logDir = logDir;
        this.inputs = test.inputs();

        List<String> cmd = buildCommandLine(test, currentOs);
        log.log(Level.FINE, "Running " + cmd);

        ProcessBuilder pb = new ProcessBuilder(cmd).inheritIO();
        if (test.output()) {
            pb.redirectOutput(createTempFile(STDOUT));
        }
        if (test.error()) {
            pb.redirectError(createTempFile(STDERR));
        }
        this.p = pb.start();

        if (cmd.get(0).endsWith("java")) {
            // Give the JVM some time to initialize
            Thread.sleep(700);
        }
    }

    public Test test() {
        return this.test;
    }

    public String[] inputs() {
        return this.inputs;
    }

    public Os currentOs() {
        return this.currentOs;
    }

    public String profilerLibPath() {
        return "build/lib/libasyncProfiler." + currentOs.getLibExt();
    }

    private List<String> buildCommandLine(Test test, Os currentOs) {
        List<String> cmd = new ArrayList<>();

        String[] sh = test.sh();
        if (sh.length > 0) {
            cmd.add("/bin/sh");
            cmd.add("-e");
            cmd.add("-c");
            cmd.add(substituteFiles(String.join(";", sh)));
        } else {
            cmd.add(System.getProperty("java.home") + "/bin/java");
            cmd.add("-cp");
            cmd.add(System.getProperty("java.class.path"));
            if (test.debugNonSafepoints()) {
                cmd.add("-XX:+UnlockDiagnosticVMOptions");
                cmd.add("-XX:+DebugNonSafepoints");
            }
            addArgs(cmd, test.jvmArgs());
            if (!test.agentArgs().isEmpty()) {
                cmd.add("-agentpath:" + profilerLibPath() + "=" +
                        substituteFiles(test.agentArgs()));
            }
            cmd.add(test.mainClass().getName());
            addArgs(cmd, test.args());
        }

        return cmd;
    }

    private String getExtFromFile(File file) {
        try (RandomAccessFile raf = new RandomAccessFile(file, "r")) {
            byte[] header = new byte[16];
            int bytes = raf.read(header);
            String s = bytes > 0 ? new String(header, 0, bytes, StandardCharsets.ISO_8859_1) : "";
            if (s.startsWith("<!DOCTYPE html>")) {
                return ".html";
            } else if (s.startsWith("FLR\0")) {
                return ".jfr";
            }
            return "";
        } catch (IOException e) {
            return "";
        }
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
            m.appendReplacement(sb, f.getPath());
        } while (m.find());

        return m.appendTail(sb).toString();
    }

    private File createTempFile(String fileId) {
        return createTempFile(fileId, null);
    }

    private File createTempFile(String fileId, String ext) {
        return tmpFiles.computeIfAbsent(fileId, key -> {
            try {
                return File.createTempFile("ap-" + key.substring(1), ext);
            } catch (IOException e) {
                throw new UncheckedIOException(e);
            }
        });
    }

    private void clearTempFiles() {
        for (File file : tmpFiles.values()) {
            file.delete();
        }
    }

    private void moveLog(File file, String name, boolean autoExtension) throws IOException {
        if (file != null) {
            String targetName = autoExtension ? name + getExtFromFile(file) : name;
            Files.move(file.toPath(), Paths.get(logDir, targetName), StandardCopyOption.REPLACE_EXISTING);
        }
    }

    private void moveLogs() {
        if (logDir == null) {
            return;
        }

        try {
            Files.createDirectories(Paths.get(logDir));

            File stdout = tmpFiles.getOrDefault(PROFOUT, tmpFiles.get(STDOUT));
            moveLog(stdout, "stdout", true);

            File stderr = tmpFiles.getOrDefault(PROFERR, tmpFiles.get(STDERR));
            moveLog(stderr, "stderr", false);

            File profile = tmpFiles.get("%f");
            moveLog(profile, "profile", true);
        } catch (IOException e) {
            log.log(Level.WARNING, "Failed to move logs", e);
        }
    }

    private boolean isRoot() {
        try (Stream<String> lines = Files.lines(Paths.get("/proc/self/status"))) {
            return lines.anyMatch(s -> s.startsWith("Uid:") && s.matches("Uid:\\s+0\\s+0.*"));
        } catch (IOException e) {
            return false;
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
            moveLogs();
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

    public int exitCode() {
        return p.exitValue();
    }

    public void waitForExit() throws TimeoutException, InterruptedException {
        waitForExit(p, timeout);
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

    public Output profile(String args) throws IOException, TimeoutException, InterruptedException {
        return profile(args, false);
    }

    public Output profile(String args, boolean sudo) throws IOException, TimeoutException, InterruptedException {
        List<String> cmd = new ArrayList<>();
        if (sudo && (new File("/usr/bin/sudo").exists() || !isRoot())) {
            cmd.add("/usr/bin/sudo");
        }
        cmd.add("build/bin/asprof");
        addArgs(cmd, args);
        cmd.add(Long.toString(pid()));
        log.log(Level.FINE, "Profiling " + cmd);

        Process p = new ProcessBuilder(cmd)
                .redirectOutput(createTempFile(PROFOUT))
                .redirectError(createTempFile(PROFERR))
                .start();

        waitForExit(p, 10);
        int exitCode = p.waitFor();
        if (exitCode != 0) {
            throw new IOException("Profiling call failed: " + readFile(PROFERR));
        }

        return readFile(PROFOUT);
    }

    public File getFile(String fileId) {
        return tmpFiles.get(fileId);
    }

    public Output readFile(String fileId) {
        File f = getFile(fileId);
        try (Stream<String> stream = Files.lines(f.toPath())) {
            return new Output(stream.toArray(String[]::new));
        } catch (IOException | UncheckedIOException e) {
            return new Output(new String[0]);
        }
    }
}
