/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;

import one.convert.Index;
import one.convert.JfrConverter;
import one.jfr.Dictionary;

public class MethodCache {
    private final JfrConverter converter;
    private final Index<String> symbolTable = new Index<>(String.class, "", 32768);
    private final Index<Method> methodIndex = new Index<>(Method.class, new Method(symbolTable.index("all"), 0), 32768);

    private final Method[] nearCache = new Method[256 * 256];
    // It should be better to create dictionary with linked methods instead of open addressed hash table
    // but in most cases all methods should fit nearCache, so less code is better
    private final Dictionary<Method> farMethods = new Dictionary<>(1024);

    public MethodCache(JfrConverter converter) {
        this.converter = converter;
    }

    public void clear() {
        Arrays.fill(nearCache, null);
        farMethods.clear();
    }

    public int index(long methodId, int location, byte type, boolean firstInStack) {
        Method method;
        if (methodId < nearCache.length) {
            int mid = (int) methodId;
            method = nearCache[mid];
            if (method == null) {
                method = createMethod(methodId, location, type, firstInStack);
                nearCache[mid] = method;
                return method.index = methodIndex.index(method);
            }
        } else {
            // this should be extremely rare case
            method = farMethods.get(methodId);
            if (method == null) {
                method = createMethod(methodId, location, type, firstInStack);
                farMethods.put(methodId, method);
                return method.index = methodIndex.index(method);
            }
        }

        Method last = null;
        Method prototype = null;
        while (method != null) {
            if (method.originalMethodId == methodId) {
                if (method.location == location && method.type == type && method.start == firstInStack) {
                    return method.index;
                }
                prototype = method;
            }
            last = method;
            method = method.next;
        }

        if (prototype != null) {
            last.next = method = new Method(methodId, prototype.className, prototype.methodName, location, type, firstInStack);
            return method.index = methodIndex.index(method);
        }

        last.next = method = createMethod(methodId, location, type, firstInStack);

        return method.index = methodIndex.index(method);
    }

    public int indexForClass(int extra, byte type) {
        long methodId = (long) extra << 32 | 1L << 63;
        Method method = farMethods.get(methodId);
        Method last = null;
        while (method != null) {
            if (method.originalMethodId == methodId) {
                if (method.location == -1 && method.type == type && !method.start) {
                    return method.index;
                }
            }
            last = method;
            method = method.next;
        }

        String javaClassName = converter.getClassName(extra);
        method = new Method(methodId, symbolTable.index(javaClassName), 0, -1, type, false);
        if (last == null) {
            farMethods.put(methodId, method);
        } else {
            last.next = method;
        }
        return method.index = methodIndex.index(method);
    }

    private Method createMethod(long methodId, int location, byte type, boolean firstInStack) {
        StackTraceElement ste = converter.getStackTraceElement(methodId, type, location);
        int className = symbolTable.index(ste.getClassName());
        int methodName = symbolTable.index(ste.getMethodName());
        return new Method(methodId, className, methodName, location, type, firstInStack);
    }

    public String[] orderedSymbolTable() {
        return symbolTable.keys();
    }

    public Index<Method> methodsIndex() {
        return methodIndex;
    }
}
