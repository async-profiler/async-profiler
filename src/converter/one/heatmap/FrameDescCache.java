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

public class FrameDescCache {
    private final JfrConverter converter;
    private final Index<String> symbolTable = new Index<>(String.class, "", 32768);
    private final Index<FrameDesc> methodIndex = new Index<>(FrameDesc.class, new FrameDesc(symbolTable.index("all"), 0), 32768);

    private final FrameDesc[] nearCache = new FrameDesc[256 * 256];
    // It should be better to create dictionary with linked methods instead of open addressed hash table
    // but in most cases all methods should fit nearCache, so less code is better
    private final Dictionary<LinkedList<FrameDesc>> farMethods = new Dictionary<>(1024);

    public FrameDescCache(JfrConverter converter) {
        this.converter = converter;
    }

    public void clear() {
        Arrays.fill(nearCache, null);
        farMethods.clear();
    }

    public int index(long methodId, int location, byte type, boolean firstInStack) {
        if (methodId < nearCache.length) {
            int mid = (int) methodId;
            FrameDesc frameDesc = nearCache[mid];
            if (frameDesc == null) {
                frameDesc = createMethod(methodId, location, type, firstInStack);
                nearCache[mid] = frameDesc;
                return frameDesc.index = methodIndex.index(frameDesc);
            }
        } else {
            // this should be extremely rare case
            LinkedList<FrameDesc> list = getFarMethodList(methodId);
            if (list.isEmpty()) {
                FrameDesc frameDesc = createMethod(methodId, location, type, firstInStack);
                list.add(frameDesc);
                frameDesc.index = methodIndex.index(frameDesc);
                return frameDesc.index;
            }
        }

        FrameDesc prototype = null;
        LinkedList<FrameDesc> list = getFarMethodList(methodId);
        for (FrameDesc frameDesc : list) {
            if (frameDesc.originalMethodId == methodId) {
                if (frameDesc.location == location && frameDesc.type == type && frameDesc.start == firstInStack) {
                    return frameDesc.index;
                }
                prototype = frameDesc;
                break;
            }
        }

        FrameDesc frameDesc;
        if (prototype == null) {
            frameDesc = createMethod(methodId, location, type, firstInStack);
        } else {
            frameDesc = new FrameDesc(methodId, prototype.className, prototype.name, location, type, firstInStack);
        }
        frameDesc.index = methodIndex.index(frameDesc);
        list.add(frameDesc);
        return frameDesc.index;
    }

    public int indexForClass(int extra, byte type) {
        long methodId = (long) extra << 32 | 1L << 63;
        LinkedList<FrameDesc> list = getFarMethodList(methodId);
        for (FrameDesc frameDesc : list) {
            if (frameDesc.originalMethodId == methodId) {
                if (frameDesc.location == -1 && frameDesc.type == type && !frameDesc.start) {
                    return frameDesc.index;
                }
            }
        }

        String javaClassName = converter.getClassName(extra);
        FrameDesc frameDesc = new FrameDesc(methodId, symbolTable.index(javaClassName), 0, -1, type, false);
        frameDesc.index = methodIndex.index(frameDesc);
        list.add(frameDesc);
        return frameDesc.index;
    }

    private FrameDesc createMethod(long methodId, int location, byte type, boolean firstInStack) {
        StackTraceElement ste = converter.getStackTraceElement(methodId, type, location);
        int className = symbolTable.index(ste.getClassName());
        int methodName = symbolTable.index(ste.getMethodName());
        return new FrameDesc(methodId, className, methodName, location, type, firstInStack);
    }

    public String[] orderedSymbolTable() {
        return symbolTable.keys();
    }

    public Index<FrameDesc> methodsIndex() {
        return methodIndex;
    }

    private LinkedList<FrameDesc> getFarMethodList(long methodId) {
        LinkedList<FrameDesc> list = farMethods.get(methodId);
        if (list == null) {
            list = new LinkedList<>();
            farMethods.put(methodId, list);
        }
        return list;
    }
}
