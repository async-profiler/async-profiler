/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.profiler.test;

import java.lang.annotation.ElementType;
import java.lang.annotation.Repeatable;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

@Target(ElementType.METHOD)
@Retention(RetentionPolicy.RUNTIME)
@Repeatable(Tests.class)
public @interface Test {

    Class<?> mainClass();

    String args() default "";

    String agentArgs() default "";

    String jvmArgs() default "";

    boolean debugNonSafepoints() default false;

    boolean output() default false;

    boolean error() default false;

    Os[] os() default {};

    int[] jdkVersion() default {};

    Jvm[] jvm() default {};

    Arch[] arch() default {};

    boolean enabled() default true;
}
