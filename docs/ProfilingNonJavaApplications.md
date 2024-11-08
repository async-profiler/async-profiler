# Profiling Non-Java applications

The scope of profiling non-java applications is limited to the case when profiler is controlled
programmatically from the process being profiled and with LD_PRELOAD. It is worth noting that 
[dynamic attach](https://github.com/async-profiler/async-profiler/blob/master/docs/OtherUseCases.md#launching-as-an-agent) 
which is available for Java is not supported for non-Java profiling.


## C API
Similar to the
[Java API](https://github.com/async-profiler/async-profiler/blob/master/docs/OtherUseCases.md#using-java-api),
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
To use it in a C/C++ application, include asprof.h. Below is an example usage showing how to use async-profiler command with the API. The :
```
int main() {
    dlerror();
    void* lib = dlopen(path/to/libasyncProfiler.so", RTLD_NOW);
    if (lib == NULL) {
        printf("%s\n", dlerror());
        dlclose(lib);
        exit(1);
    }

    asprof_init_t asprof_init = dlsym(lib, "asprof_init");
    if(asprof_init == NULL) {
        printf("%s\n", dlerror());
        dlclose(lib);
        exit(1);
    }
    
    asprof_init();

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "start,event=cpu,loglevel=debug,file=profile.jfr");

    printf("Starting profiler\n");
    
    asprof_execute_t asprof_execute = dlsym(lib, "asprof_execute");
    if(asprof_execute == NULL) {
        printf("%s\n", dlerror());
        dlclose(lib);
        exit(1);
    }

    asprof_error_t err = asprof_execute(cmd, NULL);
    if (err != NULL) {
        fprintf(stderr, "%s\n", err);
        exit(1);
    }

    // some meaningful work

    printf("Stopping profiler\n");
    err = asprof_execute("stop", NULL);
    if (err != NULL) {
        fprintf(stderr, "%s\n", err);
        exit(1);
    }

    return 0;
}
```

In addition, async-profiler can be injected into a native application through LD_PRELOAD mechanism:
```
LD_PRELOAD=/path/to/libasyncProfiler.so ASPROF_COMMAND=start,event=cpu,file=profile.jfr NativeApp [args]
```

All basic functionality remains the same. Profiler can run in cpu, wall and other perf_events 
modes. Flame Graph and JFR output formats are supported, although JFR files will obviously lack
Java-specific events.
