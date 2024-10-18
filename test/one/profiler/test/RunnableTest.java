/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.lang.reflect.Method;

public class RunnableTest {
    private final Method m;
    private final Test test;

    RunnableTest(Method m, Test test) {
        this.m = m;
        this.test = test;
    }

    public Method method() {
        return m;
    }

    public Test test() {
        return test;
    }

    public String testName() {
        return className() + '.' + m.getName();
    }

    public String className() {
        return m.getDeclaringClass().getSimpleName();
    }

    public String testInfo() {
        return testName() +
                (!test().args().isEmpty() ? " args: " + test().args() : "") +
                (test().inputs().length > 0 ? " inputs: [" + String.join(" ", test().inputs()) + "]" : "");
    }
}
