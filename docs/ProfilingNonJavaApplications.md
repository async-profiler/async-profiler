# Profiling Non-Java applications

The scope of profiling non-Java applications is limited to the case when profiler is controlled
programmatically from the process being profiled or with LD_PRELOAD. It is worth noting that
[dynamic attach](IntegratingAsyncProfiler.md#launching-as-an-agent)
which is available for Java is not supported for non-Java profiling.

## C API

Similar to the
[Java API](IntegratingAsyncProfiler.md#using-java-api),
there is a C API for using profiler inside a native application.

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
// for the profiler output. Returns an error code or NULL on success.
DLLEXPORT asprof_error_t asprof_execute(const char* command, asprof_writer_t output_callback);
typedef asprof_error_t (*asprof_execute_t)(const char* command, asprof_writer_t output_callback);
```

To use it in a C/C++ application, include `asprof.h`. Below is an example showing how to invoke async-profiler with the API:

```
#include "asprof.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

void test_output_callback(const char* buffer, size_t size) {
    fwrite(buffer, sizeof(char), size, stderr);
}

int main() {
    void* lib = dlopen("/path/to/libasyncProfiler.so", RTLD_NOW);
    if (lib == NULL) {
        printf("%s\n", dlerror());
        exit(1);
    }

    asprof_init_t asprof_init = (asprof_init_t)dlsym(lib, "asprof_init");
    asprof_execute_t asprof_execute = (asprof_execute_t)dlsym(lib, "asprof_execute");
    asprof_error_str_t asprof_error_str = (asprof_error_str_t)dlsym(lib, "asprof_error_str");

    if (asprof_init == NULL || asprof_execute == NULL || asprof_error_str == NULL) {
        printf("%s\n", dlerror());
        dlclose(lib);
        exit(1);
    }

    asprof_init();

    printf("Starting profiler\n");

    char cmd[] = "start,event=cpu,loglevel=debug,file=profile.jfr";
    asprof_error_t err = asprof_execute(cmd, test_output_callback);
    if (err != NULL) {
        fprintf(stderr, "%s\n", asprof_error_str(err));
        exit(1);
    }

    // ... some meaningful work ...

    printf("Stopping profiler\n");

    err = asprof_execute("stop", test_output_callback);
    if (err != NULL) {
        fprintf(stderr, "%s\n", asprof_error_str(err));
        exit(1);
    }

    return 0;
}
```

In addition, async-profiler can be injected into a native application through LD_PRELOAD mechanism:

```
LD_PRELOAD=/path/to/libasyncProfiler.so ASPROF_COMMAND=start,event=cpu,file=profile.jfr NativeApp [args]
```

All basic functionality remains the same. Profiler can run in `cpu`, `wall` and other perf_events
modes. Flame Graph and JFR output formats are supported, although JFR files will obviously lack
Java-specific events.
