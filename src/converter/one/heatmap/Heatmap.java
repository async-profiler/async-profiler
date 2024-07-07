/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

package one.heatmap;

import java.io.PrintStream;
import one.convert.Arguments;
import one.jfr.ClassRef;
import one.jfr.Dictionary;
import one.jfr.MethodRef;
import one.jfr.DictionaryInt;
import one.jfr.Index;
import one.convert.ResourceProcessor;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Arrays;
import java.util.Comparator;

public class Heatmap {

    private final Arguments arguments;

    private State state;

    private long startMs;

    public Heatmap(Arguments arguments) {
        this.arguments = arguments;

        // TODO it is probably should be an argument
        // but there is a good chance that changing it will have some side effects
        this.state = new State(20);
    }

    public void assignConstantPool(
            Dictionary<MethodRef> methodRefs,
            Dictionary<ClassRef> classRefs,
            Dictionary<byte[]> symbols
    ) {
        state.methodsCache.assignConstantPool(methodRefs, classRefs, symbols);
    }

    public void nextFile() {
        state.stackTracesCache.clear();
        state.methodsCache.clear();
    }

    public void addEvent(int stackTraceId, int extra, byte type, long timeMs) {
        state.addEvent(stackTraceId, extra, type, timeMs);
    }

    public void addStack(long id, long[] methods, int[] locations, byte[] types, int size) {
        state.addStack(id, methods, locations, types, size);
    }

    public void finish(long startMs) {
        this.startMs = startMs;
        assignConstantPool(null, null, null);
        nextFile();
    }

    private EvaluationContext evaluate() {
        State state = this.state;
        this.state = null;
        return new EvaluationContext(
                state.sampleList.samples(),
                state.methodsCache.methodsIndex(),
                state.stackTracesRemap.orderedTraces(),
                state.methodsCache.orderedSymbolTable()
        );
    }

    private void compressMethods(HtmlOut out, Method[] methods) throws IOException {
        for (Method method : methods) {
            out.write18(method.className);
            out.write18(method.methodName);
            out.write18(method.location & 0xffff);
            out.write18(method.location >>> 16);
            out.write6(method.type);
        }
    }

    public void dump(OutputStream stream) throws IOException {
        HtmlOut out = new HtmlOut(stream);

        EvaluationContext evaluationContext = evaluate();

        String tail = ResourceProcessor.getResource("/heatmap.html");

        tail = printTill(out, tail, "/*executionsHeatmap:*/");
        out.resetPos();
        printHeatmap(out, evaluationContext);

        tail = printTill(out, tail, "/*methods:*/");
        printMethods(out, evaluationContext);

        tail = printTill(out, tail, "/*title:*/");
        out.write((arguments.title == null ? "Heatmap" : arguments.title).getBytes());

        tail = printTill(out, tail, "/*startMs:*/0");
        out.write(String.valueOf(startMs).getBytes());

        tail = printTill(out, tail, "/*cpool:*/");
        printConstantPool(out.asPrintableStream(), evaluationContext);

        out.write(tail.getBytes());
    }

    private void printHeatmap(final HtmlOut out, EvaluationContext context) throws IOException {
        int veryStart = out.pos();
        int wasPos = out.pos();

        // calculates methods frequency during building the tree
        int[] stackChunksBuffer = buildLz78TreeAndPrepareData(context);

        // gives methods new ids, more frequent (in tree's data) methods will have lower id
        renameMethodsByFrequency(context);

        // writes "starts" - ids of methods that indicates a start of a next stack trace
        writeStartMethods(out, context);
        wasPos = debugStep("start methods", out, wasPos, veryStart);

        // writes block sizes, compressed by huffman algorithm
        writeBlockSizes(out, context);
        wasPos = debugStep("stack sizes", out, wasPos, veryStart);

        // NOTE: destroys internal state!
        SynonymTable synonymTable = context.nodeTree.extractSynonymTable();
        synonymTable.calculateSynonyms();
        // writes frequent lz tree nodes as a synonyms table
        writeSynonymsTable(out, synonymTable);
        wasPos = debugStep("tree synonyms", out, wasPos, veryStart);

        // writes lz tree with two pairs of var-ints: [parent node id] + [method id of this node]
        writeTree(out, synonymTable, context);
        wasPos = debugStep("tree body", out, wasPos, veryStart);

        // calculate counts for the next synonyms table, that will be used for samples
        int chunksCount = calculateSamplesSynonyms(synonymTable, context, stackChunksBuffer);
        // writes frequent lz tree nodes as a synonyms table (for sample chunks)
        writeSynonymsTable(out, synonymTable);
        wasPos = debugStep("samples synonyms", out, wasPos, veryStart);

        // writes sample chunks as var-ints references for [node id]
        writeSamples(out, synonymTable, context, stackChunksBuffer);
        debugStep("samples body", out, wasPos, veryStart);
        debug("storage size: " + context.nodeTree.storageSize());

        out.write30(context.nodeTree.nodesCount());
        out.write30(context.sampleList.blockSizes.length);
        out.write30(context.nodeTree.storageSize());
        out.write30(chunksCount);
        out.write30(context.sampleList.stackIds.length);
    }

    private void writeSamples(HtmlOut out, SynonymTable synonymTable, EvaluationContext context, int[] stackChunksBuffer) throws IOException {
        for (int stackId : context.sampleList.stackIds) {
            int chunksStart = stackChunksBuffer[stackId * 2];
            int chunksEnd = stackChunksBuffer[stackId * 2 + 1];

            for (int i = chunksStart; i < chunksEnd; i++) {
                int nodeId = stackChunksBuffer[i];
                out.writeVar(synonymTable.nodeIdOrSynonym(nodeId));
            }
        }
    }

    private int calculateSamplesSynonyms(SynonymTable synonymTable, EvaluationContext context, int[] stackChunksBuffer) {
        int chunksCount = 0;
        int[] childrenCount = synonymTable.reset();

        for (int stackId : context.sampleList.stackIds) {
            int chunksStart = stackChunksBuffer[stackId * 2];
            int chunksEnd = stackChunksBuffer[stackId * 2 + 1];

            for (int i = chunksStart; i < chunksEnd; i++) {
                childrenCount[stackChunksBuffer[i]]--; // negation for reverse sort
                chunksCount++;
            }
        }

        synonymTable.calculateSynonyms();
        return chunksCount;
    }

    private void writeTree(HtmlOut out, SynonymTable synonymTable, EvaluationContext context) throws IOException {
        long[] data = context.nodeTree.treeData();
        int dataSize = context.nodeTree.treeDataSize();
        for (int i = 0; i < dataSize; i++) {
            long d = data[i];
            int parentId = context.nodeTree.extractParentId(d);
            int methodId = context.nodeTree.extractMethodId(d);

            out.writeVar(synonymTable.nodeIdOrSynonym(parentId));
            out.writeVar(context.orderedMethods[methodId - 1].frequencyOrNewMethodId);
        }
    }

    private void writeSynonymsTable(HtmlOut out, SynonymTable synonymTable) throws IOException {
        out.writeVar(synonymTable.synonymsCount());
        for (int i = 0; i < synonymTable.synonymsCount(); i++) {
            out.writeVar(synonymTable.synonymAt(i));
        }
    }

    private void writeStartMethods(HtmlOut out, EvaluationContext context) throws IOException {
        int startsCount = 0;
        for (Method method : context.orderedMethods) {
            if (method.start) {
                startsCount++;
            }
        }
        out.writeVar(startsCount);
        for (Method method : context.orderedMethods) {
            if (method.start) {
                out.writeVar(method.frequencyOrNewMethodId);
            }
        }
    }

    private void renameMethodsByFrequency(EvaluationContext context) {
        Arrays.sort(context.orderedMethods, new Comparator<Method>() {
            @Override
            public int compare(Method o1, Method o2) {
                return Integer.compare(o2.frequencyOrNewMethodId, o1.frequencyOrNewMethodId);
            }
        });

        for (int i = 0; i < context.orderedMethods.length; i++) {
            Method method = context.orderedMethods[i];
            method.frequencyOrNewMethodId = i + 1; // zero is reserved for no method
        }

        // restores order
        context.methods.orderedKeys(context.orderedMethods);
    }

    private int[] buildLz78TreeAndPrepareData(EvaluationContext context) {
        int[] samples = context.sampleList.stackIds;

        // prepared data for output, firstly used to remember last stack positions
        int[] stackBuffer = new int[(context.stackTraces.length + 1) * 16];

        // remember the last position of stackId
        for (int i = 0; i < samples.length; i++) {
            int stackId = samples[i];
            stackBuffer[stackId * 2] = ~i;   // rewrites data multiple times, the last one wins
        }

        int chunksIterator = context.stackTraces.length * 2 + 1;

        // builds the tree and prepares data for the last stack
        for (int i = 0; i < samples.length; i++) {
            int stackId = samples[i];
            int current = 0;
            int[] stack = context.stackTraces[stackId];

            if (i == ~stackBuffer[stackId * 2]) {    // last version of that stack
                stackBuffer[stackId * 2] = chunksIterator;  // start

                for (int methodId : stack) {
                    current = context.nodeTree.appendChild(current, methodId);
                    if (current == 0) { // so we are starting from root again, it will be written to output as Lz78 element - [parent node id; method id]
                        context.orderedMethods[methodId - 1].frequencyOrNewMethodId++;
                        if (stackBuffer.length == chunksIterator) {
                            stackBuffer = Arrays.copyOf(stackBuffer, chunksIterator + chunksIterator / 2);
                        }

                        int justAppendedId = context.nodeTree.nodesCount() - 1;
                        stackBuffer[chunksIterator++] = justAppendedId;
                        context.nodeTree.markNodeAsLastlyUsed(justAppendedId);
                    }
                }

                if (current != 0) {
                    if (stackBuffer.length == chunksIterator) {
                        stackBuffer = Arrays.copyOf(stackBuffer, chunksIterator + chunksIterator / 2);
                    }

                    stackBuffer[chunksIterator++] = current;
                    context.nodeTree.markNodeAsLastlyUsed(current);
                }

                stackBuffer[stackId * 2 + 1] = chunksIterator;  // end
            } else { // general case
                for (int methodId : stack) {
                    current = context.nodeTree.appendChild(current, methodId);
                    if (current == 0) { // so we are starting from root again, it will be written to output as Lz78 element - [parent node id; method id]
                        context.orderedMethods[methodId - 1].frequencyOrNewMethodId++;
                    }
                }
            }
        }

        // removes unused chunks
        context.nodeTree.compactTree(stackBuffer, context.stackTraces.length * 2 + 1, chunksIterator);

        return stackBuffer;
    }

    private void writeBlockSizes(HtmlOut out, EvaluationContext context) throws IOException {
        int[] blockSizeFrequencies = new int[1024];
        int maxBlockSize = 0;
        for (int blockSize : context.sampleList.blockSizes) {
            if (blockSize >= blockSizeFrequencies.length) {
                blockSizeFrequencies = Arrays.copyOf(blockSizeFrequencies, blockSize * 2);
            }
            blockSizeFrequencies[blockSize]++;
            maxBlockSize = Math.max(maxBlockSize, blockSize);
        }

        HuffmanEncoder encoder = new HuffmanEncoder(blockSizeFrequencies, maxBlockSize);

        long[] decodeTable = encoder.calculateOutputTable();

        out.writeVar(decodeTable.length);
        int maxBits = (int) (decodeTable[decodeTable.length - 1] >>> 56);
        out.writeVar(maxBits);

        for (long l : decodeTable) {
            out.writeVar(l & 0x00FF_FFFF_FFFF_FFFFL);
            out.writeVar(l >>> 56);
        }

        for (int blockSize : context.sampleList.blockSizes) {
            if (encoder.append(blockSize)) {
                for (int value : encoder.values) {
                    out.nextByte(value);
                }
            }
        }
        if (encoder.flushIfNeed()) {
            for (int value : encoder.values) {
                out.nextByte(value);
            }
        }
    }

    private void printConstantPool(PrintStream out, EvaluationContext evaluationContext) throws IOException {
        for (byte[] symbol : evaluationContext.symbols) {
            out.print('"');
            out.print(new String(symbol)
                .replace("\\", "\\\\")
                .replace("\"", "\\\""));
            out.print("\",");
        }
    }

    private void printMethods(HtmlOut out, EvaluationContext evaluationContext) throws IOException {
        debug("methods count " + evaluationContext.orderedMethods.length);
        Arrays.sort(evaluationContext.orderedMethods, new Comparator<Method>() {
            @Override
            public int compare(Method o1, Method o2) {
                return Integer.compare(o1.frequencyOrNewMethodId, o2.frequencyOrNewMethodId);
            }
        });
        out.nextByte('A');
        compressMethods(out, evaluationContext.orderedMethods);
        out.nextByte('A');
    }

    private int debugStep(String step, HtmlOut out, int wasPos, int veryStartPos) {
        int nowPos = out.pos();
        debug(step + " " + (nowPos - wasPos) / (1024.0 * 1024.0) + " MB");
        debug(step + " pos in data " + (nowPos - veryStartPos));
        return nowPos;
    }

    private void debug(String text) {
        // Basically, no user will ever need that, but it will be helpful to debug broken data
        // System.out.println(text);
    }

    private static String printTill(HtmlOut out, String tail, String till) {
        return ResourceProcessor.printTill(out.asPrintableStream(), tail, till);
    }

    private static class EvaluationContext {
        final Index<Method> methods;
        final Method[] orderedMethods;
        final int[][] stackTraces;
        final byte[][] symbols;

        final SampleList.Result sampleList;

        final LzNodeTree nodeTree = new LzNodeTree();

        EvaluationContext(SampleList.Result sampleList, Index<Method> methods, int[][] stackTraces, byte[][] symbols) {
            this.sampleList = sampleList;
            this.methods = methods;
            this.stackTraces = stackTraces;
            this.symbols = symbols;

            orderedMethods = new Method[methods.size()];
            methods.orderedKeys(orderedMethods);
        }
    }

    private static class State {

        private static final int LIMIT = Integer.MAX_VALUE;

        final SampleList sampleList;
        final StackStorage stackTracesRemap = new StackStorage();

        final DictionaryInt stackTracesCache = new DictionaryInt();
        final MethodCache methodsCache = new MethodCache();

        // reusable array to (temporary) store (potentially) new stack trace
        int[] cachedStackTrace = new int[4096];

        State(long blockDurationMs) {
            sampleList = new SampleList(blockDurationMs);
        }

        public void addEvent(int stackTraceId, int extra, byte type, long timeMs) {
            if (sampleList.getRecordsCount() >= LIMIT) {
                return;
            }
            if (extra == 0) {
                sampleList.add(stackTracesCache.get(stackTraceId), timeMs);
                return;
            }

            int id = stackTracesCache.get((long) extra << 32 | stackTraceId, -1);
            if (id != -1) {
                sampleList.add(id, timeMs);
                return;
            }

            int prototypeId = stackTracesCache.get(stackTraceId);
            int[] prototype = stackTracesRemap.get(prototypeId);

            id = stackTracesRemap.indexWithPrototype(prototype, methodsCache.indexForClass(extra, type));
            stackTracesCache.put((long) extra << 32 | stackTraceId, id);

            sampleList.add(id, timeMs);
        }

        public void addStack(long id, long[] methods, int[] locations, byte[] types, int size) {
            int[] stackTrace = cachedStackTrace;
            if (stackTrace.length < size) {
                cachedStackTrace = stackTrace = new int[size * 2];
            }

            for (int i = size - 1; i >= 0; i--) {
                long methodId = methods[i];
                byte type = types[i];
                int location = locations[i];

                int index = size - 1 - i;
                boolean firstMethodInTrace = index == 0;

                stackTrace[index] = methodsCache.index(methodId, location, type, firstMethodInTrace);
            }

            stackTracesCache.put(id, stackTracesRemap.index(stackTrace, size));
        }

    }

}
