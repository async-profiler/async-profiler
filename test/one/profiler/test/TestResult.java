/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public class TestResult {
    private final TestStatus status;
    private final Throwable throwable;

    public TestResult(TestStatus status) {
        this(status, null);
    }

    public TestResult(TestStatus status, Throwable throwable) {
        if ((status == TestStatus.FAIL && throwable == null) || status != TestStatus.FAIL && throwable != null) {
            throw new IllegalArgumentException();
        }

        this.status = status;
        this.throwable = throwable;
    }

    public TestStatus status() {
        return status;
    }

    public Throwable throwable() {
        return throwable;
    }

    public static TestResult skip() {
        return new TestResult(TestStatus.SKIP);
    }

    public static TestResult pass() {
        return new TestResult(TestStatus.PASS);
    }

    public static TestResult fail(Throwable t) {
        return new TestResult(TestStatus.FAIL, t);
    }
}
