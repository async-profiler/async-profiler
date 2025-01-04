# Profiling Java in a container

async-profiler provides the ability to profile Java processes running in a Docker or LXC
container both from within a container and from the host system.

When profiling from the host, `pid` should be the Java process ID in the host
namespace. Use `ps aux | grep java` or `docker top <container>` to find
the process ID.

async-profiler should be run from the host by a privileged user - it will
automatically switch to the proper pid/mount namespace and change
user credentials to match the target process. Also make sure that
the target container can access `libasyncProfiler.so` by the same
absolute path as on the host. Alternatively, specify `--libpath` option
to override path to `libasyncProfiler.so` in a container.

By default, Docker container restricts the access to `perf_event_open`
syscall. There are 3 alternatives to allow profiling in a container:

1. You can modify the [seccomp profile](https://docs.docker.com/engine/security/seccomp/)
   or disable it altogether with `--security-opt seccomp=unconfined` option. In
   addition, `--cap-add SYS_ADMIN` may be required.
2. You can use "fdtransfer": see the help for `--fdtransfer`.
3. Last, you may fall back to `-e ctimer` profiling mode, see [Troubleshooting](Troubleshooting.md).
