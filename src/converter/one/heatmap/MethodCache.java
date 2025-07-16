/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;
import java.util.LinkedList;

import one.convert.Index;
import one.convert.JfrConverter;
import one.jfr.Dictionary;

// TODO Rename to FrameDescCache
public class MethodCache {
    private final JfrConverter converter;
    private final Index<String> symbolTable = new Index<>(String.class, "", 32768);
    private final Index<Method> methodIndex = new Index<>(Method.class, new Method(symbolTable.index("all"), 0), 32768);

    private final Method[] nearCache = new Method[256 * 256];
    // It should be better to create dictionary with linked methods instead of open addressed hash table
    // but in most cases all methods should fit nearCache, so less code is better
    private final Dictionary<LinkedList<Method>> farMethods = new Dictionary<>(1024);

    public MethodCache(JfrConverter converter) {
        this.converter = converter;
    }

    public void clear() {
        Arrays.fill(nearCache, null);
        farMethods.clear();
    }

    public int index(long methodId, int location, byte type, boolean firstInStack) {
        if (methodId < nearCache.length) {
            int mid = (int) methodId;
            Method method = nearCache[mid];
            if (method == null) {
                method = createMethod(methodId, location, type, firstInStack);
                nearCache[mid] = method;
                method.index = methodIndex.index(method);
                return method.index;
            }
        } else {
            // this should be extremely rare case
            LinkedList<Method> list = getFarMethodList(methodId);
            if (list.isEmpty()) {
                Method method = createMethod(methodId, location, type, firstInStack);
                list.add(method);
                method.index = methodIndex.index(method);
                return method.index;
            }
        }

        Method prototype = null;
        LinkedList<Method> list = getFarMethodList(methodId);
        for (Method method : list) {
            if (method.originalMethodId == methodId) {
                if (method.location == location && method.type == type && method.start == firstInStack) {
                    return method.index;
                }
                prototype = method;
                break;
            }
        }

        Method method;
        if (prototype == null) {
            method = createMethod(methodId, location, type, firstInStack);
        } else {
            method = new Method(methodId, prototype.className, prototype.name, location, type, firstInStack);
        }
        method.index = methodIndex.index(method);
        list.add(method);
        return method.index;
    }

    public int indexForClass(int extra, byte type) {
        long methodId = (long) extra << 32 | 1L << 63;
        LinkedList<Method> list = getFarMethodList(methodId);
        for (Method method : list) {
            if (method.originalMethodId == methodId && method.location == -1 && method.type == type
                    && !method.start) {
                return method.index;
            }
        }

        String javaClassName = converter.getClassName(extra);
        Method method = new Method(methodId, symbolTable.index(javaClassName), 0, -1, type, false);
        method.index = methodIndex.index(method);
        list.add(method);
        return method.index;
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

    private LinkedList<Method> getFarMethodList(long methodId) {
        LinkedList<Method> list = farMethods.get(methodId);
        if (list == null) {
            list = new LinkedList<>();
            farMethods.put(methodId, list);
        }
        return list;
    }
}
