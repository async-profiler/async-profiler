/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

import one.jfr.Dictionary;
import one.jfr.StackTrace;

class Classifier {

    private static final String
            GC = "[gc]",
            JIT = "[jit]",
            VM = "[vm]",
            VTABLE_STUBS = "[vtable_stubs]",
            NATIVE = "[native]",
            INTERPRETER = "[Interpreter]_[0]",
            C1_COMP = "[c1_comp]_[1]",
            C2_COMP = "[c2_comp]_[i]",
            ADAPTER = "[c2i_adapter]_[i]",
            CLASS_INIT = "[class::init]",
            CLASS_LOAD = "[class::load]",
            CLASS_RESOLVE = "[class::resolve]",
            CLASS_VERIFY = "[class::verify]",
            LAMBDA_INIT = "[lambda::init]";

    private final Dictionary<String> methodNames;

    Classifier(Dictionary<String> methodNames) {
        this.methodNames = methodNames;
    }

    public String getCategoryName(StackTrace stackTrace) {
        long[] methods = stackTrace.methods;
        byte[] types = stackTrace.types;

        String category;
        if ((category = detectGcJit(methods, types)) == null &&
                (category = detectClassLoading(methods, types)) == null) {
            category = detectOther(methods, types);
        }
        return category;
    }

    private String detectGcJit(long[] methods, byte[] types) {
        boolean vmThread = false;
        for (int i = types.length; --i >= 0; ) {
            if (types[i] == FlameGraph.FRAME_CPP) {
                switch (methodNames.get(methods[i])) {
                    case "CompileBroker::compiler_thread_loop":
                        return JIT;
                    case "GCTaskThread::run":
                    case "WorkerThread::run":
                        return GC;
                    case "java_start":
                    case "thread_native_entry":
                        vmThread = true;
                        break;
                }
            } else if (types[i] != FlameGraph.FRAME_NATIVE) {
                break;
            }
        }
        return vmThread ? VM : null;
    }

    private String detectClassLoading(long[] methods, byte[] types) {
        for (int i = 0; i < methods.length; i++) {
            String methodName = methodNames.get(methods[i]);
            if (methodName.equals("Verifier::verify")) {
                return CLASS_VERIFY;
            } else if (methodName.startsWith("InstanceKlass::initialize")) {
                return CLASS_INIT;
            } else if (methodName.startsWith("LinkResolver::") ||
                    methodName.startsWith("InterpreterRuntime::resolve") ||
                    methodName.startsWith("SystemDictionary::resolve")) {
                return CLASS_RESOLVE;
            } else if (methodName.endsWith("ClassLoader.loadClass")) {
                return CLASS_LOAD;
            } else if (methodName.endsWith("LambdaMetafactory.metafactory") ||
                    methodName.endsWith("LambdaMetafactory.altMetafactory")) {
                return LAMBDA_INIT;
            } else if (methodName.endsWith("table stub")) {
                return VTABLE_STUBS;
            } else if (methodName.equals("Interpreter")) {
                return INTERPRETER;
            } else if (methodName.startsWith("I2C/C2I")) {
                return i + 1 < types.length && types[i + 1] == FlameGraph.FRAME_INTERPRETED ? INTERPRETER : ADAPTER;
            }
        }
        return null;
    }

    private String detectOther(long[] methods, byte[] types) {
        boolean inJava = true;
        for (int i = 0; i < types.length; i++) {
            switch (types[i]) {
                case FlameGraph.FRAME_INTERPRETED:
                    return inJava ? INTERPRETER : NATIVE;
                case FlameGraph.FRAME_JIT_COMPILED:
                    return inJava ? C2_COMP : NATIVE;
                case FlameGraph.FRAME_INLINED:
                    inJava = true;
                    break;
                case FlameGraph.FRAME_NATIVE: {
                    String methodName = methodNames.get(methods[i]);
                    if (methodName.startsWith("JVM_") || methodName.startsWith("Unsafe_") ||
                            methodName.startsWith("MHN_") || methodName.startsWith("jni_")) {
                        return VM;
                    }
                    switch (methodName) {
                        case "call_stub":
                        case "deoptimization":
                        case "unknown_Java":
                        case "not_walkable_Java":
                        case "InlineCacheBuffer":
                            return VM;
                    }
                    if (methodName.endsWith("_arraycopy") || methodName.contains("pthread_cond")) {
                        break;
                    }
                    inJava = false;
                    break;
                }
                case FlameGraph.FRAME_CPP: {
                    String methodName = methodNames.get(methods[i]);
                    if (methodName.startsWith("Runtime1::")) {
                        return C1_COMP;
                    }
                    break;
                }
                case FlameGraph.FRAME_C1_COMPILED:
                    return inJava ? C1_COMP : NATIVE;
            }
        }
        return NATIVE;
    }
}
