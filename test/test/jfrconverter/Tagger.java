/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package test.jfrconverter;

import one.profiler.Span;

final class Tagger {

    private static final long SLEEP_DURATION_MS = 200;

    private static void showcase0() throws InterruptedException {
        Thread.sleep(SLEEP_DURATION_MS);
    }

    private static void showcase1() throws InterruptedException {
        Thread.sleep(SLEEP_DURATION_MS);
    }

    private static void showcase2() throws InterruptedException {
        Thread.sleep(SLEEP_DURATION_MS);
    }

    public static void main(String[] args) throws InterruptedException {
        // Distinct methods let the converter test identify samples retained for each tag.
        long span = Span.start();
        showcase0();
        Span.end(span, "showcase0");

        span = Span.start();
        showcase1();
        Span.end(span, "showcase1");

        span = Span.start();
        showcase2();
        Span.end(span, "showcase2");
    }
}
