/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

import one.convert.*;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;

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
            } else {
                throw new IllegalArgumentException("Unrecognized output format: " + args.output);
            }
        } else {
            FlameGraph.convert(input, output, args);
        }
    }

    private static String getFileName(String fileName) {
        return fileName.substring(fileName.lastIndexOf(File.separatorChar) + 1);
    }

    private static String replaceExt(String fileName, String ext) {
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
                "  -o --output FORMAT    Output format: html, collapsed, pprof, pb.gz\n" +
                "\n" +
                "JFR options:\n" +
                "     --cpu              CPU profile\n" +
                "     --wall             Wall clock profile\n" +
                "     --alloc            Allocation profile\n" +
                "     --live             Live object profile\n" +
                "     --lock             Lock contention profile\n" +
                "  -t --threads          Split stack traces by threads\n" +
                "  -s --state LIST       Filter thread states: runnable, sleeping\n" +
                "     --classify         Classify samples into predefined categories\n" +
                "     --total            Accumulate total value (time, bytes, etc.)\n" +
                "     --lines            Show line numbers\n" +
                "     --bci              Show bytecode indices\n" +
                "     --simple           Simple class names instead of FQN\n" +
                "     --norm             Normalize names of hidden classes / lambdas\n" +
                "     --dot              Dotted class names\n" +
                "     --from TIME        Start time in ms (absolute or relative)\n" +
                "     --to TIME          End time in ms (absolute or relative)\n" +
                "\n" +
                "Flame Graph options:\n" +
                "     --title STRING     Flame Graph title\n" +
                "     --minwidth X       Skip frames smaller than X%\n" +
                "     --grain X          Coarsen Flame Graph to the given grain size\n" +
                "     --skip N           Skip N bottom frames\n" +
                "  -r --reverse          Reverse stack traces (icicle graph)\n" +
                "  -I --include REGEX    Include only stacks with the specified frames\n" +
                "  -X --exclude REGEX    Exclude stacks with the specified frames\n" +
                "     --highlight REGEX  Highlight frames matching the given pattern\n");
    }
}
