/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "fdtransferServer.h"

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif


#define APP_BINARY "asprof"

static const char VERSION_STRING[] =
    "Async-profiler " PROFILER_VERSION " built on " __DATE__ "\n";

static const char USAGE_STRING[] =
    "Usage: " APP_BINARY " [action] [options] <pid>\n"
    "Actions:\n"
    "  start             start profiling and return immediately\n"
    "  resume            resume profiling without resetting collected data\n"
    "  stop              stop profiling\n"
    "  dump              dump collected data without stopping profiling session\n"
    "  check             check if the specified profiling event is available\n"
    "  status            print profiling status\n"
    "  meminfo           print profiler memory stats\n"
    "  list              list profiling events supported by the target JVM\n"
    "  load              load agent library (jattach action)\n"
    "  jcmd              run JVM diagnostic command (jattach action)\n"
    "  collect           collect profile for the specified period of time\n"
    "                    and then stop (default action)\n"
    "Options:\n"
    "  -e event          profiling event: cpu|alloc|lock|cache-misses etc.\n"
    "  -d duration       run profiling for <duration> seconds\n"
    "  -f filename       dump output to <filename>\n"
    "  -i interval       sampling interval in nanoseconds\n"
    "  -j jstackdepth    maximum Java stack depth\n"
    "  -t, --threads     profile different threads separately\n"
    "  -s, --simple      simple class names instead of FQN\n"
    "  -n, --norm        normalize names of hidden classes / lambdas\n"
    "  -g, --sig         print method signatures\n"
    "  -a, --ann         annotate Java methods\n"
    "  -l, --lib         prepend library names\n"
    "  -o fmt            output format: flat|traces|collapsed|flamegraph|tree|jfr\n"
    "  -I include        output only stack traces containing the specified pattern\n"
    "  -X exclude        exclude stack traces with the specified pattern\n"
    "  -L level          log level: debug|info|warn|error|none\n"
    "  -F features       advanced stack trace features: vtable, comptask, pcaddr\n"
    "  -v, --version     display version string\n"
    "\n"
    "  --title string    FlameGraph title\n"
    "  --minwidth pct    skip frames smaller than pct%%\n"
    "  --reverse         generate stack-reversed FlameGraph / Call tree\n"
    "\n"
    "  --loop time       run profiler in a loop\n"
    "  --alloc bytes     allocation profiling interval in bytes\n"
    "  --live            build allocation profile from live objects only\n"
    "  --lock duration   lock profiling threshold in nanoseconds\n"
    "  --wall interval   wall clock profiling interval\n"
    "  --total           accumulate the total value (time, bytes, etc.)\n"
    "  --all-user        only include user-mode events\n"
    "  --sched           group threads by scheduling policy\n"
    "  --cstack mode     how to traverse C stack: fp|dwarf|lbr|vm|no\n"
    "  --signal num      use alternative signal for cpu or wall clock profiling\n"
    "  --clock source    clock source for JFR timestamps: tsc|monotonic\n"
    "  --begin function  begin profiling when function is executed\n"
    "  --end function    end profiling when function is executed\n"
    "  --ttsp            time-to-safepoint profiling\n"
    "  --jfropts opts    JFR recording options: mem\n"
    "  --jfrsync config  synchronize profiler with JFR recording\n"
    "  --libpath path    full path to libasyncProfiler.so in the container\n"
    "  --fdtransfer      use fdtransfer to serve perf requests\n"
    "                    from the non-privileged target\n"
    "\n"
    "<pid> is a numeric process ID of the target JVM\n"
    "      or 'jps' keyword to find running JVM automatically\n"
    "      or the application name as it would appear in the jps tool\n"
    "\n"
    "Example: " APP_BINARY " -d 30 -f profile.html 3456\n"
    "         " APP_BINARY " start -i 1ms jps\n"
    "         " APP_BINARY " stop -o flat jps\n"
    "         " APP_BINARY " -d 5 -e alloc MyAppName\n";


extern "C" int jattach(int pid, int argc, const char** argv, int print_output);

static void error(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void error(const char* msg, int errnum) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errnum));
    exit(1);
}


class Args {
  private:
    int _argc;
    const char** _argv;

  public:
    Args(int argc, const char** argv) : _argc(argc - 1), _argv(argv) {
    }

    int count() const { return _argc; }

    const char** array() const { return _argv + 1; }

    const char* next() {
        if (count() <= 0) {
            error("Missing required parameter");
        }
        _argc--;
        _argv++;
        return _argv[0];
    }
};


class String {
  private:
    char* _str;

  public:
    String(const char* str = "") {
        _str = strdup(str);
    }

    String(const String& other) {
        _str = strdup(other._str);
    }

    ~String() {
        free(_str);
    }

    String& operator=(const String& other) {
        free(_str);
        _str = strdup(other._str);
        return *this;
    }

    const char* str() const {
        return _str;
    }

    bool operator==(const char* other) const {
        return strcmp(_str, other) == 0;
    }

    bool operator==(const String& other) const {
        return strcmp(_str, other._str) == 0;
    }

    String& operator<<(const char* tail) {
        size_t len = strlen(_str);
        _str = (char*)realloc(_str, len + strlen(tail) + 1);
        strcpy(_str + len, tail);
        return *this;
    }

    String& operator<<(String& tail) {
        return operator<<(tail._str);
    }

    String& operator<<(int n) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", n);
        return operator<<(buf);
    }

    String& replace(char c, const char* rep) {
        const char* start = _str;
        const char* p;
        while ((p = strchr(start, c)) != NULL) {
            size_t rep_len = strlen(rep);
            char* tmp = (char*)malloc(strlen(_str) + rep_len);
            memcpy(tmp, _str, p - _str);
            strcpy(tmp + (p - _str), rep);
            start = strcpy(tmp + (p - _str) + rep_len, p + 1);
            free(_str);
            _str = tmp;
        }
        return *this;
    }
};


static String action = "collect";
static String file, logfile, output, params, format, fdtransfer, libpath;
static bool jattach_action = false;
static bool use_tmp_file = false;
static int duration = 60;
static int pid = 0;
static volatile unsigned long long end_time;

static void sigint_handler(int sig) {
    end_time = 0;
}

static unsigned long long time_micros() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)tv.tv_sec * 1000000 + tv.tv_usec;
}

static void setup_output_files(int pid) {
    char current_dir[1024];
    int self_pid = getpid();

    if (file == "") {
        file = String("/tmp/asprof.") << self_pid << "." << pid;
        use_tmp_file = true;
    } else if (file.str()[0] != '/' && getcwd(current_dir, sizeof(current_dir)) != NULL) {
        file = String(current_dir) << "/" << file;
    }

    // The agent recognizes temporary log name, see Arguments::hasTemporaryLog()
    logfile = String("/tmp/asprof-log.") << self_pid << "." << pid;
}

static void setup_lib_path() {
    char buf[1024];

#ifdef __linux__
    const char* lib = "../lib/libasyncProfiler.so";
    char* exe = realpath("/proc/self/exe", NULL);
    if (exe == NULL) {
        // realpath() may fail for a path like /proc/[pid]/root/bin/asprof
        // In this case, resolve the link as is and do not check for .so existence
        ssize_t size = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (size >= 0) {
            buf[size] = 0;
            exe = buf;
        }
    }
#elif defined(__APPLE__)
    const char* lib = "../lib/libasyncProfiler.dylib";
    uint32_t size = sizeof(buf);
    char* exe = _NSGetExecutablePath(buf, &size) == 0 ? realpath(buf, NULL) : NULL;
#endif

    char* slash;
    if (exe != NULL && (slash = strrchr(exe, '/')) != NULL) {
        slash[1] = 0;
        libpath = String(exe) << lib;
    }

    if (exe != buf) {
        free(exe);
        struct stat statbuf;
        if (stat(libpath.str(), &statbuf) != 0 || !S_ISREG(statbuf.st_mode)) {
            libpath = "asyncProfiler";
        }
    }
}

static void print_file(String file, int dst) {
    int src = open(file.str(), O_RDONLY);
    if (src < 0 && errno == ENOENT) {
        file = String("/proc/") << pid << "/root" << file;
        src = open(file.str(), O_RDONLY);
    }
    if (src >= 0) {
        char buf[8192];
        ssize_t bytes;
        while ((bytes = read(src, buf, sizeof(buf))) > 0) {
            bytes = write(dst, buf, bytes);
        }
        close(src);
        unlink(file.str());
    }
}

static int wait_for_exit(int pid) {
    int ret = 0;
    while (waitpid(pid, &ret, 0) < 0 && errno == EINTR);
    return ret;
}

static int jps(const char* cmd, const char* app_name = NULL) {
    FILE* pipe = popen(cmd, "r");
    if (pipe == NULL) {
        error("Failed to execute jps", errno);
    }

    int pid = 0;
    char* line = NULL;
    size_t size = 0;
    ssize_t len;

    while ((len = getline(&line, &size, pipe)) > 0) {
        line[--len] = 0;
        if (app_name == NULL || ((len -= strlen(app_name)) > 0 &&
                                 line[len - 1] == ' ' &&
                                 strcmp(line + len, app_name) == 0)) {
            if (pid != 0) {
                error("Multiple Java processes found");
            }
            pid = atoi(line);
        }
    }

    free(line);
    pclose(pipe);

    if (pid == 0) {
        error("No Java process");
    }
    return pid;
}

static void run_fdtransfer(int pid, String& fdtransfer) {
    if (!FdTransferServer::supported() || fdtransfer == "") return;

    pid_t child = fork();
    if (child == -1) {
        error("fork failed", errno);
    }

    if (child == 0) {
        exit(FdTransferServer::runOnce(pid, fdtransfer.str()) ? 0 : 1);
    } else {
        int ret = wait_for_exit(child);
        if (ret != 0) {
            exit(WEXITSTATUS(ret));
        }
    }
}

static void run_jattach(int pid, String& cmd) {
    pid_t child = fork();
    if (child == -1) {
        error("fork failed", errno);
    }

    if (child == 0) {
        const char* argv[] = {"load", libpath.str(), libpath.str()[0] == '/' ? "true" : "false", cmd.str()};
        exit(jattach(pid, 4, argv, 0));
    } else {
        int ret = wait_for_exit(child);
        if (ret != 0) {
            print_file(logfile, STDERR_FILENO);
            exit(WEXITSTATUS(ret));
        }

        print_file(logfile, STDERR_FILENO);
        if (use_tmp_file) print_file(file, STDOUT_FILENO);
    }
}

// Resolve a kernel tracepoint name (e.g. syscalls:sys_enter_openat) to a numerical id
static int get_tracepoint_id(const char* dir, const char* name) {
    char buf[256];
    if ((size_t)snprintf(buf, sizeof(buf), "/sys/kernel/%s/events/%s/id", dir, name) >= sizeof(buf)) {
        return 0;
    }

    char* p = strchr(buf, ':');
    if (p == NULL || strchr(p + 1, ':') != NULL) {
        return 0;
    }
    *p = '/';

    int fd = open(buf, O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    ssize_t r = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (r > 0) {
        buf[r] = 0;
        return atoi(buf);
    }
    return 0;
}


int main(int argc, const char** argv) {
    Args args(argc, argv);
    while (args.count() > 0 && !(jattach_action && pid)) {
        String arg = args.next();

        if (arg == "start" || arg == "resume" || arg == "stop" || arg == "dump" || arg == "check" ||
            arg == "status" || arg == "meminfo" || arg == "list" || arg == "collect") {
            action = arg;

        } else if (arg == "load" || arg == "jcmd" || arg == "threaddump" || arg == "dumpheap" || arg == "inspectheap") {
            action = arg;
            jattach_action = true;

        } else if (arg == "-h" || arg == "--help") {
            printf(USAGE_STRING);
            return 0;

        } else if (arg == "-v" || arg == "--version") {
            printf(VERSION_STRING);
            return 0;

        } else if (arg == "-d") {
            duration = atoi(args.next());

        } else if (arg == "-f") {
            file = args.next();

        } else if (arg == "-o") {
            output = args.next();

        } else if (arg == "-e" || arg == "--event") {
            const char* event = args.next();
            int tracepoint_id;
            if (strchr(event, ',') != NULL && event[strlen(event) - 1] == '/') {
                // PMU event, e.g.: cpu/umask=0x1,event=0xd3/
                params << ",event=" << String(event).replace(',', ":");
            } else if (strchr(event, ':') != NULL &&
                       ((tracepoint_id = get_tracepoint_id("tracing", event)) > 0 ||
                        (tracepoint_id = get_tracepoint_id("debug/tracing", event)) > 0)) {
                // Try to resolve tracepoint id before dropping root privileges
                params << ",event=trace:" << tracepoint_id;
            } else {
                params << ",event=" << event;
            }

        } else if (arg == "-i" || arg == "--interval") {
            params << ",interval=" << args.next();

        } else if (arg == "-j" || arg == "--jstackdepth") {
            params << ",jstackdepth=" << args.next();

        } else if (arg == "-t" || arg == "--threads") {
            params << ",threads";

        } else if (arg == "-s" || arg == "--simple") {
            format << ",simple";

        } else if (arg == "-n" || arg == "--norm") {
            format << ",norm";

        } else if (arg == "-g" || arg == "--sig") {
            format << ",sig";

        } else if (arg == "-a" || arg == "--ann") {
            format << ",ann";

        } else if (arg == "-l" || arg == "--lib") {
            format << ",lib";

        } else if (arg == "-I" || arg == "--include") {
            format << ",include=" << args.next();

        } else if (arg == "-X" || arg == "--exclude") {
            format << ",exclude=" << args.next();

        } else if (arg == "-L" || arg == "--log") {
            format << ",loglevel=" << args.next();

        } else if (arg == "-F" || arg == "--features") {
            format << ",features=" << String(args.next()).replace(',', "+");

        } else if (arg == "--filter") {
            format << ",filter=" << String(args.next()).replace(',', ";");

        } else if (arg == "--title") {
            format << ",title=" << String(args.next()).replace('&', "&amp;")
                                                      .replace('<', "&lt;")
                                                      .replace('>', "&gt;")
                                                      .replace(',', "&#44;");

        } else if (arg == "--width" || arg == "--height" || arg == "--minwidth") {
            format << "," << (arg.str() + 2) << "=" << args.next();

        } else if (arg == "--reverse" || arg == "--samples" || arg == "--total" || arg == "--sched" || arg == "--live") {
            format << "," << (arg.str() + 2);

        } else if (arg == "--alloc" || arg == "--lock" || arg == "--wall" ||
                   arg == "--chunksize" || arg == "--chunktime" ||
                   arg == "--cstack" || arg == "--signal" || arg == "--clock" || arg == "--begin" || arg == "--end") {
            params << "," << (arg.str() + 2) << "=" << args.next();

        } else if (arg == "--ttsp") {
            params << ",begin=SafepointSynchronize::begin,end=RuntimeService::record_safepoint_synchronized";

        } else if (arg == "--all-user") {
            params << ",alluser";

        } else if (arg == "--safe-mode") {
            params << ",safemode=" << args.next();

        } else if (arg == "--jfrsync") {
            params << ",jfrsync=" << args.next();
            output = "jfr";

        } else if (arg == "--timeout" || arg == "--loop") {
            params << "," << (arg.str() + 2) << "=" << args.next();
            if (action == "collect") action = "start";

        } else if (arg == "--libpath") {
            libpath = args.next();

        } else if (arg == "--fdtransfer") {
            char buf[64];
            snprintf(buf, sizeof(buf), "@asprof-%d-%08x", getpid(), (unsigned int)time_micros());
            fdtransfer = buf;
            params << ",fdtransfer=" << fdtransfer;

        } else if (arg.str()[0] >= '0' && arg.str()[0] <= '9' && pid == 0) {
            pid = atoi(arg.str());

        } else if (arg == "jps" && pid == 0) {
            // A shortcut for getting PID of a running Java application.
            // -XX:+PerfDisableSharedMem prevents jps from appearing in its own list
            pid = jps("pgrep -n java || jps -q -J-XX:+PerfDisableSharedMem");

        } else if (arg.str()[0] != '-' && args.count() == 0 && pid == 0) {
            // The last argument is the application name as it would appear in the jps tool
            pid = jps("jps -J-XX:+PerfDisableSharedMem", arg.str());

        } else {
            fprintf(stderr, "Unrecognized option: %s\n", arg.str());
            return 1;
        }
    }

    if (pid == 0) {
        printf(USAGE_STRING);
        return 1;
    }

    if (jattach_action) {
        argc = args.count() + 1;
        argv = (const char**)alloca(argc * sizeof(char*));
        argv[0] = action.str();
        memcpy(&argv[1], args.array(), (argc - 1) * sizeof(char*));
        return jattach(pid, argc, argv, 1);
    }

    setup_output_files(pid);
    if (libpath == "") {
        setup_lib_path();
    }

    if (action == "collect") {
        run_fdtransfer(pid, fdtransfer);
        run_jattach(pid, String("start,file=") << file << "," << output << format << params << ",log=" << logfile);

        fprintf(stderr, "Profiling for %d seconds\n", duration);
        end_time = time_micros() + duration * 1000000ULL;
        signal(SIGINT, sigint_handler);
        signal(SIGTERM, sigint_handler);

        while (time_micros() < end_time) {
            if (kill(pid, 0) != 0) {
                fprintf(stderr, "Process exited\n");
                if (use_tmp_file) print_file(file, STDOUT_FILENO);
                return 0;
            }
            sleep(1);
        }

        fprintf(stderr, end_time != 0 ? "Done\n" : "Interrupted\n");
        signal(SIGINT, SIG_DFL);
        // Do not reset SIGTERM handler to allow graceful shutdown

        run_jattach(pid, String("stop,file=") << file << "," << output << format << ",log=" << logfile);
    } else {
        if (action == "start" || action == "resume") run_fdtransfer(pid, fdtransfer);
        run_jattach(pid, String(action) << ",file=" << file << "," << output << format << params << ",log=" << logfile);
    }

    return 0;
}
