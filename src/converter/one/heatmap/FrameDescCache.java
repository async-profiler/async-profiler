/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.util.Arrays;

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
    private final Dictionary<FrameDesc> farMethods = new Dictionary<>(1024);

    public FrameDescCache(JfrConverter converter) {
        this.converter = converter;
    }

    public void clear() {
        Arrays.fill(nearCache, null);
        farMethods.clear();
    }

    public int index(long methodId, int location, byte type, boolean firstInStack) {
        FrameDesc frameDesc;
        if (methodId < nearCache.length) {
            int mid = (int) methodId;
            frameDesc = nearCache[mid];
            if (frameDesc == null) {
                frameDesc = createMethod(methodId, location, type, firstInStack);
                nearCache[mid] = frameDesc;
                return frameDesc.index = methodIndex.index(frameDesc);
            }
        } else {
            // this should be extremely rare case
            frameDesc = farMethods.get(methodId);
            if (frameDesc == null) {
                frameDesc = createMethod(methodId, location, type, firstInStack);
                farMethods.put(methodId, frameDesc);
                return frameDesc.index = methodIndex.index(frameDesc);
            }
        }

        FrameDesc last = null;
        FrameDesc prototype = null;
        while (frameDesc != null) {
            if (frameDesc.originalMethodId == methodId) {
                if (frameDesc.location == location && frameDesc.type == type && frameDesc.start == firstInStack) {
                    return frameDesc.index;
                }
                prototype = frameDesc;
            }
            last = frameDesc;
            frameDesc = frameDesc.next;
        }

        if (prototype != null) {
            last.next = frameDesc = new FrameDesc(methodId, prototype.className, prototype.name, location, type, firstInStack);
            return frameDesc.index = methodIndex.index(frameDesc);
        }

        last.next = frameDesc = createMethod(methodId, location, type, firstInStack);

        return frameDesc.index = methodIndex.index(frameDesc);
    }

    public int indexForClass(int extra, byte type) {
        long methodId = (long) extra << 32 | 1L << 63;
        FrameDesc frameDesc = farMethods.get(methodId);
        FrameDesc last = null;
        while (frameDesc != null) {
            if (frameDesc.originalMethodId == methodId) {
                if (frameDesc.location == -1 && frameDesc.type == type && !frameDesc.start) {
                    return frameDesc.index;
                }
            }
            last = frameDesc;
            frameDesc = frameDesc.next;
        }

        String javaClassName = converter.getClassName(extra);
        frameDesc = new FrameDesc(methodId, symbolTable.index(javaClassName), 0, -1, type, false);
        if (last == null) {
            farMethods.put(methodId, frameDesc);
        } else {
            last.next = frameDesc;
        }
        return frameDesc.index = methodIndex.index(frameDesc);
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
}
