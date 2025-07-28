/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;
import java.util.Optional;
import java.util.function.Supplier;

import one.convert.Frame;
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

    public int index(long methodId, int location, byte type, boolean start) {
        return index(methodId, location, type, start,
                () -> createStackTraceMethod(methodId, location, type, start));
    }

    public int indexForClass(int extra, byte type) {
        long methodId = (long) extra << 32 | type;
        return index(methodId, -1, type, false,
                () -> new Method(methodId, symbolTable.index(converter.getClassName(extra)), 0, -1, type, false));
    }

    public int indexForThread(String threadName) {
        long methodId = (long) threadName.hashCode() << 32 | Frame.TYPE_NATIVE;
        return index(methodId, -1, Frame.TYPE_NATIVE, true,
                () -> new Method(methodId, 0, symbolTable.index(threadName), -1, Frame.TYPE_NATIVE, true));
    }

    public int index(long methodId, int location, byte type, boolean start, Supplier<Method> methodSupplier) {
        Optional<Method> methodOpt = findMethodById(methodId);
        if (!methodOpt.isPresent()) {
            return putMethod(methodSupplier.get());
        }
        Method head = methodOpt.get();
        return traverseAndAppend(head, methodId, location, type, start);
    }

    private Optional<Method> findMethodById(long id) {
        if (id < nearCache.length) {
            return Optional.ofNullable(nearCache[(int) id]);
        } else {
            return Optional.ofNullable(farMethods.get(id));
        }
    }

    private TraverseResult findMatch(Method current, int location, byte type, boolean start) {
        Method last = null;
        while (current != null) {
            if (current.location == location && current.type == type && current.start == start) {
                return new TraverseResult(current, null);
            }
            last = current;
            current = current.next;
        }
        return new TraverseResult(null, last);
    }

    private int putMethod(Method method) {
        long id = method.originalMethodId;
        if (id < nearCache.length) {
            nearCache[(int) id] = method;
        } else {
            farMethods.put(id, method);
        }
        return method.index = methodIndex.index(method);
    }

    private int traverseAndAppend(Method head, long methodId, int location, byte type, boolean start) {
        TraverseResult traverseResult = findMatch(head, location, type, start);
        if (traverseResult.match != null) {
            return traverseResult.match.index;
        }

        Method method = new Method(methodId, head.className, head.methodName, location, type, start);
        method.index = methodIndex.index(method);
        traverseResult.last.next = method;
        return method.index;
    }

    private Method createStackTraceMethod(long methodId, int location, byte type, boolean firstInStack) {
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

    private static final class TraverseResult {
        private final Method match;
        // ID matches, but other parts don't (e.g. `firstInStack`)
        private final Method last;

        public TraverseResult(Method match, Method last) {
            this.match = match;
            this.last = last;
        }
    }
}
