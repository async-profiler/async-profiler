/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.wall;

// Compiled code calling an interpreted method (excluded from compilation) with many locals, so that samples often land
// in the interpreter's method entry while it zeroes the locals, before it has pushed its frame. With
// -XX:+PreserveFramePointer, VM stack walking then reads a slot of the compiled caller's frame as the Method* and
// faults (#1829), which exercises StackWalker's crash protection.
public class InterpreterEntryApp {
    static volatile Object sink;

    static long callee(Object a, Object b) {
        long l00 = 0, l01 = 0, l02 = 0, l03 = 0, l04 = 0, l05 = 0, l06 = 0, l07 = 0, l08 = 0, l09 = 0;
        long l10 = 0, l11 = 0, l12 = 0, l13 = 0, l14 = 0, l15 = 0, l16 = 0, l17 = 0, l18 = 0, l19 = 0;
        long l20 = 0, l21 = 0, l22 = 0, l23 = 0, l24 = 0, l25 = 0, l26 = 0, l27 = 0, l28 = 0, l29 = 0;
        long l30 = 0, l31 = 0, l32 = 0, l33 = 0, l34 = 0, l35 = 0, l36 = 0, l37 = 0, l38 = 0, l39 = 0;
        long l40 = 0, l41 = 0, l42 = 0, l43 = 0, l44 = 0, l45 = 0, l46 = 0, l47 = 0, l48 = 0, l49 = 0;
        long l50 = 0, l51 = 0, l52 = 0, l53 = 0, l54 = 0, l55 = 0, l56 = 0, l57 = 0, l58 = 0, l59 = 0;
        long l60 = 0, l61 = 0, l62 = 0, l63 = 0, l64 = 0, l65 = 0, l66 = 0, l67 = 0, l68 = 0, l69 = 0;
        long l70 = 0, l71 = 0, l72 = 0, l73 = 0, l74 = 0, l75 = 0, l76 = 0, l77 = 0, l78 = 0, l79 = 0;
        long l80 = 0, l81 = 0, l82 = 0, l83 = 0, l84 = 0, l85 = 0, l86 = 0, l87 = 0, l88 = 0, l89 = 0;
        long l90 = 0, l91 = 0, l92 = 0, l93 = 0, l94 = 0, l95 = 0, l96 = 0, l97 = 0, l98 = 0, l99 = 0;
        return a.hashCode() ^ b.hashCode() ^ l00 ^ l99;
    }

    // Keeps several oops live across the call, so that they sit in the compiled frame's spill slots
    static long caller(Object[] objs, int n) {
        long acc = 0;
        for (int i = 0; i < n; i++) {
            Object a = objs[i & 7], b = objs[(i + 3) & 7], c = objs[(i + 5) & 7];
            acc += callee(a, b);
            acc += c.hashCode() + a.hashCode();
        }
        return acc;
    }

    public static void main(String[] args) throws Exception {
        for (int t = 0; t < 4; t++) {
            new Thread(() -> {
                Object[] objs = new Object[8];
                for (int i = 0; i < 8; i++) objs[i] = new int[]{i};
                while (true) sink = caller(objs, 100_000);
            }, "worker-" + t).start();
        }
    }
}
