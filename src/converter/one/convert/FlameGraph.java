/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.convert;

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;
import java.util.Comparator;
import java.util.StringTokenizer;
import java.util.regex.Pattern;

import static one.convert.Frame.*;

public class FlameGraph implements Comparator<Frame> {
    private static final Frame[] EMPTY_FRAME_ARRAY = {};
    private static final String[] FRAME_SUFFIX = {"_[0]", "_[j]", "_[i]", "", "", "_[k]", "_[1]"};
    private static final byte HAS_SUFFIX = (byte) 0x80;
    private static final int FLUSH_THRESHOLD = 15000;

    private final Arguments args;
    private final Index<String> cpool = new Index<>(String.class, "");
    private final Frame root = new Frame(0, TYPE_NATIVE);
    private final StringBuilder outbuf = new StringBuilder(FLUSH_THRESHOLD + 1000);
    private int[] order;
    private int depth;
    private int lastLevel;
    private long lastX;
    private long lastTotal;
    private long mintotal;

    public FlameGraph(Arguments args) {
        this.args = args;
    }

    public void parseCollapsed(Reader in) throws IOException {
        CallStack stack = new CallStack();

        try (BufferedReader br = new BufferedReader(in)) {
            for (String line; (line = br.readLine()) != null; ) {
                int space = line.lastIndexOf(' ');
                if (space <= 0) continue;

                long ticks = Long.parseLong(line.substring(space + 1));

                for (int from = 0, to; from < space; from = to + 1) {
                    if ((to = line.indexOf(';', from)) < 0) to = space;
                    String name = line.substring(from, to);
                    byte type = detectType(name);
                    if ((type & HAS_SUFFIX) != 0) {
                        name = name.substring(0, name.length() - 4);
                        type ^= HAS_SUFFIX;
                    }
                    stack.push(name, type);
                }

                addSample(stack, ticks);
                stack.clear();
            }
        }
    }

    public void parseHtml(Reader in) throws IOException {
        Frame[] levels = new Frame[128];
        int level = 0;
        long total = 0;
        boolean needRebuild = args.reverse || args.include != null || args.exclude != null;

        try (BufferedReader br = new BufferedReader(in)) {
            while (!br.readLine().startsWith("const cpool")) ;
            br.readLine();

            String s = "";
            for (String line; (line = br.readLine()).startsWith("'"); ) {
                String packed = unescape(line.substring(1, line.lastIndexOf('\'')));
                s = s.substring(0, packed.charAt(0) - ' ').concat(packed.substring(1));
                cpool.put(s, cpool.size());
            }

            while (!br.readLine().isEmpty()) ;

            for (String line; !(line = br.readLine()).isEmpty(); ) {
                StringTokenizer st = new StringTokenizer(line.substring(2, line.length() - 1), ",");
                int nameAndType = Integer.parseInt(st.nextToken());

                char func = line.charAt(0);
                if (func == 'f') {
                    level = Integer.parseInt(st.nextToken());
                    st.nextToken();
                } else if (func == 'u') {
                    level++;
                } else if (func != 'n') {
                    throw new IllegalStateException("Unexpected line: " + line);
                }

                if (st.hasMoreTokens()) {
                    total = Long.parseLong(st.nextToken());
                }

                int titleIndex = nameAndType >>> 3;
                byte type = (byte) (nameAndType & 7);
                if (st.hasMoreTokens() && (type <= TYPE_INLINED || type >= TYPE_C1_COMPILED)) {
                    type = TYPE_JIT_COMPILED;
                }

                Frame f = level > 0 || needRebuild ? new Frame(titleIndex, type) : root;
                f.self = f.total = total;
                if (st.hasMoreTokens()) f.inlined = Long.parseLong(st.nextToken());
                if (st.hasMoreTokens()) f.c1 = Long.parseLong(st.nextToken());
                if (st.hasMoreTokens()) f.interpreted = Long.parseLong(st.nextToken());

                if (level > 0) {
                    Frame parent = levels[level - 1];
                    parent.put(f.key, f);
                    parent.self -= total;
                    depth = Math.max(depth, level);
                }
                if (level >= levels.length) {
                    levels = Arrays.copyOf(levels, level * 2);
                }
                levels[level] = f;
            }
        }

        if (needRebuild) {
            rebuild(levels[0], new CallStack(), cpool.keys());
        }
    }

    private void rebuild(Frame frame, CallStack stack, String[] strings) {
        if (frame.self > 0) {
            addSample(stack, frame.self);
        }
        if (!frame.isEmpty()) {
            for (Frame child : frame.values()) {
                stack.push(strings[child.getTitleIndex()], child.getType());
                rebuild(child, stack, strings);
                stack.pop();
            }
        }
    }

    public void addSample(CallStack stack, long ticks) {
        if (excludeStack(stack)) {
            return;
        }

        Frame frame = root;
        if (args.reverse) {
            for (int i = stack.size; --i >= args.skip; ) {
                frame = addChild(frame, stack.names[i], stack.types[i], ticks);
            }
        } else {
            for (int i = args.skip; i < stack.size; i++) {
                frame = addChild(frame, stack.names[i], stack.types[i], ticks);
            }
        }
        frame.total += ticks;
        frame.self += ticks;

        depth = Math.max(depth, stack.size);
    }

    public void dump(PrintStream out) {
        mintotal = (long) (root.total * args.minwidth / 100);

        if ("collapsed".equals(args.output)) {
            printFrameCollapsed(out, root, cpool.keys());
            return;
        }

        String tail = getResource("/flame.html");

        tail = printTill(out, tail, "/*height:*/300");
        int depth = mintotal > 1 ? root.depth(mintotal) : this.depth + 1;
        out.print(Math.min(depth * 16, 32767));

        tail = printTill(out, tail, "/*title:*/");
        out.print(args.title);

        tail = printTill(out, tail, "/*reverse:*/false");
        out.print(args.reverse);

        tail = printTill(out, tail, "/*depth:*/0");
        out.print(depth);

        tail = printTill(out, tail, "/*cpool:*/");
        printCpool(out);

        tail = printTill(out, tail, "/*frames:*/");
        printFrame(out, root, 0, 0);
        out.print(outbuf);

        tail = printTill(out, tail, "/*highlight:*/");
        out.print(args.highlight != null ? "'" + escape(args.highlight) + "'" : "");

        out.print(tail);
    }

    private String printTill(PrintStream out, String data, String till) {
        int index = data.indexOf(till);
        out.print(data.substring(0, index));
        return data.substring(index + till.length());
    }

    private void printCpool(PrintStream out) {
        String[] strings = cpool.keys();
        Arrays.sort(strings);
        out.print("'all'");

        order = new int[strings.length];
        String s = "";
        for (int i = 1; i < strings.length; i++) {
            int prefixLen = Math.min(getCommonPrefix(s, s = strings[i]), 95);
            out.print(",\n'" + escape((char) (prefixLen + ' ') + s.substring(prefixLen)) + "'");
            order[cpool.get(s)] = i;
        }

        // cpool is not used beyond this point
        cpool.clear();
    }

    private void printFrame(PrintStream out, Frame frame, int level, long x) {
        int nameAndType = order[frame.getTitleIndex()] << 3 | frame.getType();
        boolean hasExtraTypes = (frame.inlined | frame.c1 | frame.interpreted) != 0 &&
                frame.inlined < frame.total && frame.interpreted < frame.total;

        char func = 'f';
        if (level == lastLevel + 1 && x == lastX) {
            func = 'u';
        } else if (level == lastLevel && x == lastX + lastTotal) {
            func = 'n';
        }

        StringBuilder sb = outbuf.append(func).append('(').append(nameAndType);
        if (func == 'f') {
            sb.append(',').append(level).append(',').append(x - lastX);
        }
        if (frame.total != lastTotal || hasExtraTypes) {
            sb.append(',').append(frame.total);
            if (hasExtraTypes) {
                sb.append(',').append(frame.inlined).append(',').append(frame.c1).append(',').append(frame.interpreted);
            }
        }
        sb.append(")\n");

        if (sb.length() > FLUSH_THRESHOLD) {
            out.print(sb);
            sb.setLength(0);
        }

        lastLevel = level;
        lastX = x;
        lastTotal = frame.total;

        Frame[] children = frame.values().toArray(EMPTY_FRAME_ARRAY);
        Arrays.sort(children, this);

        x += frame.self;
        for (Frame child : children) {
            if (child.total >= mintotal) {
                printFrame(out, child, level + 1, x);
            }
            x += child.total;
        }
    }

    private void printFrameCollapsed(PrintStream out, Frame frame, String[] strings) {
        StringBuilder sb = outbuf;
        int prevLength = sb.length();

        if (frame != root) {
            sb.append(strings[frame.getTitleIndex()]).append(FRAME_SUFFIX[frame.getType()]);
            if (frame.self > 0) {
                int tmpLength = sb.length();
                out.print(sb.append(' ').append(frame.self).append('\n'));
                sb.setLength(tmpLength);
            }
            sb.append(';');
        }

        if (!frame.isEmpty()) {
            for (Frame child : frame.values()) {
                if (child.total >= mintotal) {
                    printFrameCollapsed(out, child, strings);
                }
            }
        }

        sb.setLength(prevLength);
    }

    private boolean excludeStack(CallStack stack) {
        Pattern include = args.include;
        Pattern exclude = args.exclude;
        if (include == null && exclude == null) {
            return false;
        }

        for (int i = 0; i < stack.size; i++) {
            if (exclude != null && exclude.matcher(stack.names[i]).matches()) {
                return true;
            }
            if (include != null && include.matcher(stack.names[i]).matches()) {
                if (exclude == null) return false;
                include = null;
            }
        }

        return include != null;
    }

    private Frame addChild(Frame frame, String title, byte type, long ticks) {
        frame.total += ticks;

        int titleIndex = cpool.index(title);

        Frame child;
        switch (type) {
            case TYPE_INTERPRETED:
                (child = frame.getChild(titleIndex, TYPE_JIT_COMPILED)).interpreted += ticks;
                break;
            case TYPE_INLINED:
                (child = frame.getChild(titleIndex, TYPE_JIT_COMPILED)).inlined += ticks;
                break;
            case TYPE_C1_COMPILED:
                (child = frame.getChild(titleIndex, TYPE_JIT_COMPILED)).c1 += ticks;
                break;
            default:
                child = frame.getChild(titleIndex, type);
        }
        return child;
    }

    private static byte detectType(String title) {
        if (title.endsWith("_[j]")) {
            return TYPE_JIT_COMPILED | HAS_SUFFIX;
        } else if (title.endsWith("_[i]")) {
            return TYPE_INLINED | HAS_SUFFIX;
        } else if (title.endsWith("_[k]")) {
            return TYPE_KERNEL | HAS_SUFFIX;
        } else if (title.endsWith("_[0]")) {
            return TYPE_INTERPRETED | HAS_SUFFIX;
        } else if (title.endsWith("_[1]")) {
            return TYPE_C1_COMPILED | HAS_SUFFIX;
        } else if (title.contains("::") || title.startsWith("-[") || title.startsWith("+[")) {
            return TYPE_CPP;
        } else if (title.indexOf('/') > 0 && title.charAt(0) != '['
                || title.indexOf('.') > 0 && Character.isUpperCase(title.charAt(0))) {
            return TYPE_JIT_COMPILED;
        } else {
            return TYPE_NATIVE;
        }
    }

    private static int getCommonPrefix(String a, String b) {
        int length = Math.min(a.length(), b.length());
        for (int i = 0; i < length; i++) {
            if (a.charAt(i) != b.charAt(i) || a.charAt(i) > 127) {
                return i;
            }
        }
        return length;
    }

    private static String escape(String s) {
        if (s.indexOf('\\') >= 0) s = s.replace("\\", "\\\\");
        if (s.indexOf('\'') >= 0) s = s.replace("'", "\\'");
        return s;
    }

    private static String unescape(String s) {
        if (s.indexOf('\'') >= 0) s = s.replace("\\'", "'");
        if (s.indexOf('\\') >= 0) s = s.replace("\\\\", "\\");
        return s;
    }

    private static String getResource(String name) {
        try (InputStream stream = FlameGraph.class.getResourceAsStream(name)) {
            if (stream == null) {
                throw new IOException("No resource found");
            }

            ByteArrayOutputStream result = new ByteArrayOutputStream();
            byte[] buffer = new byte[32768];
            for (int length; (length = stream.read(buffer)) != -1; ) {
                result.write(buffer, 0, length);
            }
            return result.toString("UTF-8");
        } catch (IOException e) {
            throw new IllegalStateException("Can't load resource with name " + name);
        }
    }

    @Override
    public int compare(Frame f1, Frame f2) {
        return order[f1.getTitleIndex()] - order[f2.getTitleIndex()];
    }

    public static void convert(String input, String output, Arguments args) throws IOException {
        FlameGraph fg = new FlameGraph(args);
        try (InputStreamReader in = new InputStreamReader(new FileInputStream(input), StandardCharsets.UTF_8)) {
            if (input.endsWith(".html")) {
                fg.parseHtml(in);
            } else {
                fg.parseCollapsed(in);
            }
        }
        try (PrintStream out = new PrintStream(output, "UTF-8")) {
            fg.dump(out);
        }
    }
}
