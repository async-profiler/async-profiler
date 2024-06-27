package one.heatmap;

import one.jfr.ClassRef;
import one.jfr.Dictionary;
import one.jfr.MethodRef;
import one.util.Frames;
import one.util.Index;

import java.util.Arrays;

public class MethodCache {

    private static final byte[] LAMBDA = "Lambda$".getBytes();
    private static final int UNKNOWN_ID = -1;
    private static final MethodRef UNKNOWN_METHOD_REF = new MethodRef(UNKNOWN_ID, UNKNOWN_ID, UNKNOWN_ID);
    private static final ClassRef UNKNOWN_CLASS_REF = new ClassRef(UNKNOWN_ID);

    private Dictionary<MethodRef> methodRefs;
    private Dictionary<ClassRef> classRefs;
    private Dictionary<byte[]> symbols;

    private final SymbolTable symbolTable = new SymbolTable();
    private final int emptyIndex = symbolTable.index(new byte[0]);
    private final Index<Method> methodIndex = new Index<>();
    {
        methodIndex.index(new Method(symbolTable.index("all".getBytes()), emptyIndex));
    }

    private final Method[] nearCache = new Method[256 * 256];
    // It should be better to create dictionary with linked methods instead of open addressed hash table
    // but in most cases all methods should fit nearCache, so less code is better
    private final Dictionary<Method> farMethods = new Dictionary<>(1024 * 1024);

    public void assignConstantPool(
            Dictionary<MethodRef> methodRefs,
            Dictionary<ClassRef> classRefs,
            Dictionary<byte[]> symbols
    ) {
        clear();
        this.methodRefs = methodRefs;
        this.classRefs = classRefs;
        this.symbols = symbols;
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
        if (method != null) {
            while (method != null) {
                if (method.originalMethodId == methodId) {
                    if (method.location == -1 && method.type == type && !method.start) {
                        return method.index;
                    }
                }
                last = method;
                method = method.next;
            }
        }

        ClassRef classRef = classRefs.get(extra);
        byte[] classNameBytes = this.symbols.get(classRef == null ? UNKNOWN_CLASS_REF.name : classRef.name);
        method = new Method(methodId, symbolTable.indexWithPostTransform(classNameBytes), emptyIndex, -1, type, false);
        if (last == null) {
            farMethods.put(methodId, method);
        } else {
            last.next = method;
        }
        return method.index = methodIndex.index(method);
    }

    private Method createMethod(long methodId, int location, byte type, boolean firstInStack) {
        MethodRef methodRef = methodRefs.get(methodId);
        if (methodRef == null) {
            methodRef = UNKNOWN_METHOD_REF;
        }

        ClassRef classRef = classRefs.get(methodRef.cls);
        if (classRef == null) {
            classRef = UNKNOWN_CLASS_REF;
        }

        byte[] classNameBytes = this.symbols.get(classRef.name);
        byte[] methodNameBytes = this.symbols.get(methodRef.name);

        int className = isNativeFrame(type)
                ? symbolTable.index(classNameBytes)
                : symbolTable.indexWithPostTransform(transformIfNeeded(classNameBytes));
        int methodName = symbolTable.index(methodNameBytes);

        return new Method(methodId, className, methodName, location, type, firstInStack);
    }

    private byte[] transformIfNeeded(byte[] className) {
        a: for (int i = 0; i < className.length - LAMBDA.length; i++) {
            if (className[i] == '$') {
                i++;
                for (int j = 0; j < LAMBDA.length; j++, i++) {
                    if (className[i] != LAMBDA[j]) {
                        i--;
                        continue a;
                    }
                }
                return Arrays.copyOf(className, i);
            }
        }
        return className;
    }

    private boolean isNativeFrame(byte methodType) {
        return methodType >= Frames.FRAME_NATIVE && methodType <= Frames.FRAME_KERNEL;
    }

    public byte[][] orderedSymbolTable() {
        return symbolTable.orderedKeys();
    }

    public Index<Method> methodsIndex() {
        return methodIndex;
    }
}
