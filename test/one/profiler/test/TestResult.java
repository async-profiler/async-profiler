/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

public class TestResult {
    private final TestStatus status;
    private final Throwable throwable;
    private final SkipReason skipReason;

    public TestResult(TestStatus status) {
        this(status, (Throwable) null);
    }

    public TestResult(TestStatus status, Throwable throwable) {
        if ((status == TestStatus.FAIL) ^ (throwable != null)) {
            throw new IllegalArgumentException();
        }

        this.status = status;
        this.throwable = throwable;
        this.skipReason = null;
    }

    public TestResult(TestStatus status, SkipReason skipReason) {
        if ((status != TestStatus.SKIP) || (skipReason == null)) {
            throw new IllegalArgumentException();
        }

        this.status = status;
        this.throwable = null;
        this.skipReason = skipReason;
    }

    public TestStatus status() {
        return status;
    }

    public Throwable throwable() {
        return throwable;
    }

    public SkipReason skipReason() {
        return skipReason;
    }

    public static TestResult skip(SkipReason reason) {
        return new TestResult(TestStatus.SKIP, reason);
    }

    public static TestResult pass() {
        return new TestResult(TestStatus.PASS);
    }

    public static TestResult fail(Throwable t) {
        return new TestResult(TestStatus.FAIL, t);
    }
}
