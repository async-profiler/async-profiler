/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

import one.convert.*;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Map;

public class Main {

    public static void main(String[] argv) throws Exception {
        Arguments args = new Arguments(argv);
        if (args.help || args.files.isEmpty()) {
            usage();
            return;
        }

        if (args.files.size() == 1) {
            args.files.add(".");
        }

        int fileCount = args.files.size() - 1;
        String lastFile = args.files.get(fileCount);
        boolean isDirectory = new File(lastFile).isDirectory();

        if (args.output == null) {
            int ext;
            if (!isDirectory && (ext = lastFile.lastIndexOf('.')) > 0) {
                args.output = lastFile.substring(ext + 1);
            } else {
                args.output = "html";
            }
        }

        if (args.differential) {
            if (fileCount < 1 || fileCount > 2 || isDirectory) {
                System.err.println("Error: --differential requires 2 input files and optionally 1 output file");
                System.err.println("Usage: jfrconv --differential <file1> <file2> [output]");
                System.exit(1);
            }

            String file1 = args.files.get(0);
            String file2 = args.files.get(1);
            String output = fileCount == 1 ? "differential-output.html" : lastFile;

            if (file1.equals(file2)) {
                System.err.println("Error: Cannot create differential flame graph with the same file twice");
                System.err.println("Provided: " + file1 + " vs " + file2);
                System.exit(1);
            }

            processDifferential(file1, file2, output, args);
            return;
        }

        for (int i = 0; i < fileCount; i++) {
            String input = args.files.get(i);
            String output = isDirectory ? new File(lastFile, replaceExt(input, args.output)).getPath() : lastFile;

            System.out.print("Converting " + getFileName(input) + " -> " + getFileName(output) + " ");
            System.out.flush();

            long startTime = System.nanoTime();
            convert(input, output, args);
            long endTime = System.nanoTime();

            System.out.print("# " + (endTime - startTime) / 1000000 / 1000.0 + " s\n");
        }
    }

    public static void convert(String input, String output, Arguments args) throws IOException {
        if (isJfr(input)) {
            if ("html".equals(args.output) || "collapsed".equals(args.output)) {
                JfrToFlame.convert(input, output, args);
            } else if ("pprof".equals(args.output) || "pb".equals(args.output) || args.output.endsWith("gz")) {
                JfrToPprof.convert(input, output, args);
            } else if ("heatmap".equals(args.output)) {
                JfrToHeatmap.convert(input, output, args);
            } else if ("otlp".equals(args.output)) {
                JfrToOtlp.convert(input, output, args);
            } else {
                throw new IllegalArgumentException("Unrecognized output format: " + args.output);
            }
        } else {
            FlameGraph.convert(input, output, args);
        }
    }

    private static void processDifferential(String file1, String file2, String output, Arguments args)
            throws IOException {
        System.out.print("Creating differential flame graph: " + getFileName(file1) + " vs " + getFileName(file2)
                + " -> " + getFileName(output) + " ");
        System.out.flush();

        long startTime = System.nanoTime();

        try {
            validateFileFormats(file1, file2);

            ProfileData profile1 = getProfileData(file1, args);
            ProfileData profile2 = getProfileData(file2, args);
            DifferentialResult differentialResult = DiffFolded.process(profile1, profile2);

            createDifferentialFlameGraph(differentialResult, output, args);
        } catch (IOException e) {
            throw e;
        } catch (Exception e) {
            throw new IOException("Failed to create differential: " + e.getMessage(), e);
        }

        long endTime = System.nanoTime();
        System.out.print("# " + (endTime - startTime) / 1000000 / 1000.0 + " s\n");
    }

    private static void validateFileFormats(String file1, String file2) throws IOException {
        String format1 = getFileFormat(file1);
        String format2 = getFileFormat(file2);

        if (!format1.equals(format2)) {
            System.err.println("Error: Cannot create differential flame graph with different file formats");
            System.err.println("File 1: " + getFileName(file1) + " (" + format1 + ")");
            System.err.println("File 2: " + getFileName(file2) + " (" + format2 + ")");
            System.err.println("Both files must be the same format (JFR, HTML, or collapsed)");
            System.exit(1);
        }
    }

    private static String getFileFormat(String inputFile) throws IOException {
        if (isJfr(inputFile))
            return "JFR";
        else if (inputFile.endsWith(".html"))
            return "HTML";
        else
            return "collapsed";
    }

    private static ProfileData getProfileData(String inputFile, Arguments args) throws IOException {
        if (isJfr(inputFile)) {
            return JfrToFlame.extractCollapsedDataWithDuration(inputFile, args);
        } else if (inputFile.endsWith(".html")) {
            Map<String, Long> collapsedData = FlameGraph.extractCollapsedDataFromHtml(inputFile, args);
            return new ProfileData(collapsedData, null);
        } else {
            Map<String, Long> collapsedData = DiffFolded.readCollapsedFile(inputFile);
            return new ProfileData(collapsedData, null);
        }
    }

    private static void createDifferentialFlameGraph(DifferentialResult differentialResult, String output,
            Arguments args) throws IOException {
        FlameGraph fg = new FlameGraph(args);
        fg.parseDifferentialData(differentialResult);

        try (FileOutputStream out = new FileOutputStream(output)) {
            try (PrintStream ps = new PrintStream(out, false, "UTF-8")) {
                fg.dump(ps);
            }
        }
    }

    private static String getFileName(String fileName) {
        return fileName.substring(fileName.lastIndexOf(File.separatorChar) + 1);
    }

    private static String getFileBaseName(String fileName) {
        String name = getFileName(fileName);
        int dot = name.lastIndexOf('.');
        return dot > 0 ? name.substring(0, dot) : name;
    }

    private static String replaceExt(String fileName, String output) {
        String ext = "heatmap".equals(output) ? "html" : output;
        int slash = fileName.lastIndexOf(File.separatorChar);
        int dot = fileName.lastIndexOf('.');
        return dot > slash ? fileName.substring(slash + 1, dot + 1) + ext : fileName.substring(slash + 1) + '.' + ext;
    }

    private static boolean isJfr(String fileName) throws IOException {
        if (fileName.endsWith(".jfr")) {
            return true;
        } else if (fileName.endsWith(".collapsed") || fileName.endsWith(".txt") || fileName.endsWith(".csv")) {
            return false;
        }
        byte[] buf = new byte[4];
        try (FileInputStream fis = new FileInputStream(fileName)) {
            return fis.read(buf) == 4 && buf[0] == 'F' && buf[1] == 'L' && buf[2] == 'R' && buf[3] == 0;
        }
    }

    private static void usage() {
        System.out.print("Usage: jfrconv [options] <input> [<input>...] <output>\n" +
                "\n" +
                "Conversion options:\n" +
                "  -o --output FORMAT    Output format: html, collapsed, pprof, pb.gz, heatmap, otlp\n" +
                "\n" +
                "JFR options:\n" +
                "     --cpu              CPU profile (ExecutionSample)\n" +
                "     --cpu-time         CPU profile (CPUTimeSample)\n" +
                "     --wall             Wall clock profile\n" +
                "     --alloc            Allocation profile\n" +
                "     --live             Live object profile\n" +
                "     --nativemem        malloc profile\n" +
                "     --leak             Only include memory leaks in nativemem\n" +
                "     --tail RATIO       Ignore tail allocations for leak profiling (10% by default)\n" +
                "     --lock             Lock contention profile\n" +
                "  -t --threads          Split stack traces by threads\n" +
                "  -s --state LIST       Filter thread states: runnable, sleeping\n" +
                "     --classify         Classify samples into predefined categories\n" +
                "     --total            Accumulate total value (time, bytes, etc.)\n" +
                "     --lines            Show line numbers\n" +
                "     --bci              Show bytecode indices\n" +
                "     --simple           Simple class names instead of FQN\n" +
                "     --norm             Normalize names of hidden classes / lambdas\n" +
                "     --differential     Enable differential mode (auto-enables --norm)\n" +
                "     --dot              Dotted class names\n" +
                "     --from TIME        Start time in ms (absolute or relative)\n" +
                "     --to TIME          End time in ms (absolute or relative)\n" +
                "\n" +
                "Flame Graph options:\n" +
                "     --title STRING     Flame Graph title\n" +
                "     --minwidth X       Skip frames smaller than X%\n" +
                "     --grain X          Coarsen Flame Graph to the given grain size\n" +
                "     --skip N           Skip N bottom frames\n" +
                "  -r --reverse          Reverse stack traces (defaults to icicle graph)\n" +
                "  -i --inverted         Toggles the layout for reversed stacktraces from icicle to flamegraph\n" +
                "                        and for default stacktraces from flamegraph to icicle\n" +
                "  -I --include REGEX    Include only stacks with the specified frames\n" +
                "  -X --exclude REGEX    Exclude stacks with the specified frames\n" +
                "     --highlight REGEX  Highlight frames matching the given pattern\n");
    }
}
