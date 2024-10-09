/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>


#ifdef __APPLE__
#  include <mach-o/dyld.h>
#  define COMMON_JVM_DIR "/Library/Java/JavaVirtualMachines/"
#  define CONTENTS_HOME  "/Contents/Home"
#else
#  define COMMON_JVM_DIR "/usr/lib/jvm/"
#  define CONTENTS_HOME  ""
#endif

#define JAVA_EXE "java"


static const char VERSION_STRING[] =
    "JFR converter " PROFILER_VERSION " built on " __DATE__ "\n";


static char exe_path[PATH_MAX];
static char java_path[PATH_MAX];

static bool get_exe_path() {
#ifdef __APPLE__
    char buf[PATH_MAX];
    uint32_t size = sizeof(buf);
    return _NSGetExecutablePath(buf, &size) == 0 && realpath(buf, exe_path) != NULL;
#else
    if (realpath("/proc/self/exe", exe_path) == NULL) {
        // realpath() may fail for a path like /proc/[pid]/root/bin/exe
        ssize_t size = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
        if (size < 0) {
            return false;
        }
        exe_path[size] = 0;
    }
    return true;
#endif
}

static const char* const* build_cmdline(int argc, char** argv) {
    const char** cmd = (const char**)malloc((argc + 6) * sizeof(char*));
    int count = 0;

    cmd[count++] = JAVA_EXE;
    cmd[count++] = "-Xss2M";
    cmd[count++] = "-Dsun.misc.URLClassPath.disableJarChecking";

    for (; argc > 0; argc--, argv++) {
        if ((strncmp(*argv, "-D", 2) == 0 || strncmp(*argv, "-X", 2) == 0) && (*argv)[2] ||
                strncmp(*argv, "-agent", 6) == 0) {
            cmd[count++] = *argv;
        } else if (strncmp(*argv, "-J", 2) == 0) {
            cmd[count++] = *argv + 2;
        } else {
            break;
        }
    }

    cmd[count++] = "-jar";
    cmd[count++] = exe_path;

    for (; argc > 0; argc--, argv++) {
        cmd[count++] = *argv;
    }

    cmd[count] = NULL;
    return cmd;
}

static bool find_java_at(const char* path, const char* path1 = "", const char* path2 = "") {
    if (snprintf(java_path, sizeof(java_path), "%s%s%s/" JAVA_EXE, path, path1, path2) >= sizeof(java_path)) {
        return false;
    }

    struct stat st;
    return stat(java_path, &st) == 0 && S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR) != 0;
}

static void run_java(char* const* cmd) {
    // 1. Get java executable from JAVA_HOME
    char* java_home = getenv("JAVA_HOME");
    if (java_home != NULL && find_java_at(java_home, "/bin")) {
        execv(java_path, cmd);
    }

    // 2. Try to find java in PATH
    execvp(JAVA_EXE, cmd);

    // 3. Try /etc/alternatives/java
    if (find_java_at("/etc/alternatives")) {
        execv(java_path, cmd);
    }

    // 4. Look for java in the common directory
    DIR* dir = opendir(COMMON_JVM_DIR);
    if (dir != NULL) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] != '.' && entry->d_type == DT_DIR) {
                if (find_java_at(COMMON_JVM_DIR, entry->d_name, CONTENTS_HOME "/bin")) {
                    execv(java_path, cmd);
                }
            }
        }
        closedir(dir);
    }
}

int main(int argc, char** argv) {
    if (argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf(VERSION_STRING);
        return 0;
    }

    if (!get_exe_path()) {
        fprintf(stderr, "Failed to get executable path\n");
        return 1;
    }

    const char* const* cmd = build_cmdline(argc - 1, argv + 1);
    run_java((char* const*)cmd);

    // May reach here only if run_java() fails
    fprintf(stderr, "No JDK found. Set JAVA_HOME or ensure java executable is on the PATH\n");
    return 1;
}
