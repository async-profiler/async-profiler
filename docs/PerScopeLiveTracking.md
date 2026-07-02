# Per-Scope Live Allocation Tracking — API & Integration Plan

## Context

This feature lets a Java application mark named regions of work ("scopes") and ask the
profiler how many bytes allocated inside each scope are **still live** in the heap. It was
prototyped as `ballast`, a standalone JVMTI agent (package `io.github.schiemon.ballast`); we
now fold the idea into async-profiler directly.

This document pins down the **API surface and dataflow** across the three layers (Java app ↔
async-profiler Java ↔ async-profiler C++ agent). Deep native data-structure design is left to
implementation.

Scope of the first cut:
- **Exclusive attribution only** — an allocation is attributed to the innermost open scope on
  the allocating thread. Shared/roll-up-to-parents is designed for but not built yet.
- **Live bytes only** — no churn/"all" total, no confidence intervals.
- **No JFR export**, no OpenTelemetry integration, no major/minor GC classification.

## Design summary

Three levels, mirroring ballast (registry → scope → handle). The read path and the scope-id
transport reuse mechanisms async-profiler already has.

### 1. Public Java API (`one.profiler`, new classes)

```java
public final class ProfilingScope {
    public static ProfilingScope register(String name); // get-or-create by name; caches; calls registerScope0() once per new name
    public String name();
    public Handle enter();                               // enterScope0(id); returns AutoCloseable
    public interface Handle extends AutoCloseable {
        void close();                                    // exitScope0(id)
    }
}
```

- `enter()`/`close()` follow ballast's rules: try-with-resources gives LIFO; double-close on
  the owning thread is idempotent; closing on another thread throws; there is **no** automatic
  cross-thread propagation (call `enter()` on the worker thread).
- **Reading** is pull-based, exposed through a small facade that triggers a dump and parses it:

```java
public final class ScopeSnapshot {   // returned by a dump call
    public long gcId();              // async-profiler _gc_id at dump time (generation / dirty marker, NOT major-only)
    public long timestamp();         // when the snapshot was taken
    public long liveBytes(ProfilingScope scope);
    public Map<ProfilingScope, Long> liveBytesByScope();
}
```

- The facade calls `AsyncProfiler.getInstance().execute("dumpScoped,live")` under the hood and
  maps `scopeId → name` (Java owns names; native never sees them). The user never touches
  command strings.
- **Enablement:** starting tracking issues `start,event=alloc,live,scopes` internally — the
  existing `live` mode plus a new `scopes` gate. Without `scopes`, there is zero per-scope cost.

### 2. async-profiler Java ↔ C++ (JNI + command)

New native methods, registered dynamically using the same pattern as the `Recording` natives:

```java
private static native long registerScope0();      // native mints a compact, contiguous id
private static native void enterScope0(long id);   // push id onto this thread's native scope stack
private static native void exitScope0(long id);    // pop
```

New command, parsed and dispatched through the single existing choke point:
- Token `dumpScoped` with metric sub-option `live`, e.g. `execute("dumpScoped,live")`.
- Returns text via the normal `execute0` String path (`BufferWriter`):
  ```
  <gcId> <timestamp>
  <scopeId> <liveBytes>
  <scopeId> <liveBytes>
  ...
  ```
- New `Arguments` flag `scopes` (mirrors `_live`).

### 3. C++ agent internals (extension points — implementation deferred)

- **Per-thread scope stack:** native maintains the thread's open-scope stack (push on
  `enterScope0`, pop on `exitScope0`); the top is the current leaf. Natural home: the
  profiler's per-thread data (`ThreadLocalData`).
- **Capture at alloc:** in the sampled-alloc path, read the thread's leaf scope id and store it
  with the sampled object (extend the live-ref record to carry a scope id).
- **Per-scope aggregation:** keep live bytes per scope id (the contiguous id doubles as a table
  index).
- **Cleanup:** prune dead weak refs, guarded by a change in `_gc_id` (skip rescans when no GC
  happened since the last cleanup). This is the DataDog `cleanup_table()` practice.
- **Dump:** sum live bytes per scope, emit rows plus the `(gcId, timestamp)` header.

## Critical files

Java API (new + wiring):
- `src/api/one/profiler/ProfilingScope.java` (new), plus the read facade / `ScopeSnapshot`.
- `src/javaApi.cpp` + the natives table and dynamic `RegisterNatives` (model on
  `RecordingAPI::registerNatives`) — wire `registerScope0`/`enterScope0`/`exitScope0`.

Command & flag:
- `src/arguments.h` / `src/arguments.cpp` — add the `scopes` flag and the `dumpScoped` token
  (mirror `_live` / `CASE("live")`, arguments.cpp:315).
- `src/profiler.cpp` — dispatch the new dump in `runInternal` (profiler.cpp:1524); reuse
  `BufferWriter` for the String return.

Native tracking (extend, do not rewrite):
- `src/objectSampler.cpp` (`LiveRefs`, objectSampler.cpp:32-129) — the existing weak-ref
  liveness table; extend its per-entry record with a scope id and add per-scope summation.
- `src/threadLocalData.{h,cpp}` (+ `asprof_thread_local_data`, asprof.h:42) — hold the
  per-thread scope stack / leaf id.

## Reuse (existing utilities and patterns)

- Command path: `Arguments::parse` (arguments.cpp:41) → `Profiler::runInternal` (profiler.cpp:1524).
- Return path: `Java_..._execute0` String return (javaApi.cpp:59) + `BufferWriter` (writer.h).
- JNI registration: `RecordingAPI::registerNatives` + `profiler_natives[]` (javaApi.cpp).
- Liveness: `LiveRefs` weak-ref + non-null test at dump; `GarbageCollectionStart` reset
  (objectSampler.cpp:141); GC epoch `_gc_id` (profiler.cpp:149) with the `_last_gc_id` guard
  pattern already used at flightRecorder.cpp:448.

## Comparison reference — DataDog java-profiler (steal good practices)

Track `DataDog/java-profiler`, file `ddprof-lib/src/main/cpp/livenessTracker.cpp`, as we build:
- `GarbageCollectionFinish` → `onGC()`: bumps a GC epoch and records heap usage; performs **no**
  GC-type classification. Confirms our "epoch as dirty flag" approach.
- `cleanup_table()`: epoch-guarded prune, compacts survivors forward, `DeleteWeakGlobalRef` the
  dead ones; liveness via `IsSameObject(ref, nullptr)`. Model our cleanup on this.
- Note the difference: their table is a single global; ours needs a scope dimension.
- Also review their jweak overflow / realloc-failure handling for robustness.

## Explicitly deferred

Shared/roll-up attribution (API shaped to add `register(name, Mode)` + `registerScope0(int mode)`
later), churn/"all" bytes, confidence intervals, JFR export of scope data, OpenTelemetry bridge,
major-GC-only snapshots, and virtual-thread carrier-migration correctness.

## Verification (end-to-end)

1. Build: `make` (native lib + `async-profiler.jar`).
2. Functional test (new, using JOL for ground truth like ballast's `MultiThreadedWorkloadDemo`):
   - `register` two scopes; inside each, allocate arrays of known size, retain some, drop others.
   - `System.gc()`, then dump; assert per-scope `liveBytes` is in the right ballpark (sampling
     is approximate, so assert ranges, not exact bytes).
   - Assert the `gcId`/`timestamp` header is present and `gcId` advances across GCs.
3. Nesting: nested `enter()` attributes allocations to the innermost scope (exclusive).
4. Off-path: with the `scopes` flag absent, confirm no per-scope tracking occurs (plain `live`).
