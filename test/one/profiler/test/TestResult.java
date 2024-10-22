/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public class TestResult {
    private final TestStatus status;
    private final Throwable throwable;

    private TestResult(TestStatus status, Throwable throwable) {
        if ((status == TestStatus.FAIL) ^ (throwable != null)) {
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

    public static TestResult skipConfigMismatch() {
        return new TestResult(TestStatus.SKIP_CONFIG_MISMATCH, null);
    }

    public static TestResult skipDisabled() {
        return new TestResult(TestStatus.SKIP_DISABLED, null);
    }

    public static TestResult pass() {
        return new TestResult(TestStatus.PASS, null);
    }

    public static TestResult fail(Throwable t) {
        return new TestResult(TestStatus.FAIL, t);
    }
}
