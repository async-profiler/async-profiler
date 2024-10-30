# Profiling Non-Java Applications

The scope of profiling non-java applications is limited to the case when profiler is controlled
programmatically from the process being profiled. Similar to the
[Java API](https://github.com/async-profiler/async-profiler/blob/restructure_readme/docs/AlternateWaysToUseAsprof.md#using-java-api),
there is a C API for using inside native applications.
```
typedef const char* asprof_error_t;
typedef void (*asprof_writer_t)(const char* buf, size_t size);

// Should be called once prior to any other API functions
DLLEXPORT void asprof_init();
typedef void (*asprof_init_t)();

// Returns an error message for the given error code or NULL if there is no error
DLLEXPORT const char* asprof_error_str(asprof_error_t err);
typedef const char* (*asprof_error_str_t)(asprof_error_t err);

// Executes async-profiler command using output_callback as an optional sink
// for the profiler output. Returning an error code or NULL on success.
DLLEXPORT asprof_error_t asprof_execute(const char* command, asprof_writer_t output_callback);
typedef asprof_error_t (*asprof_execute_t)(const char* command, asprof_writer_t output_callback);
```

To use it in a C/C++ application, include asprof.h. An example usage can be found in a 
[test process](https://github.com/async-profiler/async-profiler/blob/master/test/test/c/nativeApi.c)
in our source code package.

In addition, async-profiler can be injected into a native application through LD_PRELOAD mechanism:
```
LD_PRELOAD=/path/to/libasyncProfiler.so ASPROF_COMMAND=start,event=cpu,file=profile.jfr NativeApp [args]
```

All basic functionality remains the same. Profiler can run in cpu, wall and other perf_events 
modes. Flame Graph and JFR output formats are supported, although JFR files will obviously lack
Java-specific events.
