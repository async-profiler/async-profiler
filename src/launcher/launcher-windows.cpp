/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define COMMON_JVM_DIR "C:\\Program Files\\Java"
#define JAVA_EXE       "java.exe"

#define PATH_MAX       MAX_PATH


static const char VERSION_STRING[] =
    "JFR converter " PROFILER_VERSION " built on " __DATE__ "\n";


static char exe_path[PATH_MAX];
static char java_path[PATH_MAX];

static bool get_exe_path() {
   return GetModuleFileName(NULL, exe_path, sizeof(exe_path)) < sizeof(exe_path);
}

static void add_arg(char*& cmd, const char* arg) {
    if (strchr(arg, ' ') == NULL) {
        cmd = strcpy(cmd, arg) + strlen(arg);
    } else {
        *cmd++ = '"';
        cmd = strcpy(cmd, arg) + strlen(arg);
        *cmd++ = '"';
    }
    *cmd++ = ' ';
}

static char* build_cmdline(int argc, char** argv) {
    static char buf[40000];

    char* cmd = buf;
    add_arg(cmd, JAVA_EXE);
    add_arg(cmd, "-Xss2M");

    for (; argc > 0; argc--, argv++) {
        if ((strncmp(*argv, "-D", 2) == 0 || strncmp(*argv, "-X", 2) == 0) && (*argv)[2] ||
                strncmp(*argv, "-agent", 6) == 0) {
            add_arg(cmd, *argv);
        } else if (strncmp(*argv, "-J", 2) == 0) {
            add_arg(cmd, *argv + 2);
        } else {
            break;
        }
    }

    add_arg(cmd, "-jar");
    add_arg(cmd, exe_path);

    for (; argc > 0; argc--, argv++) {
        add_arg(cmd, *argv);
    }

    cmd[-1] = 0;
    return buf;
}

static bool find_java_at(const char* path, const char* path1 = "", const char* path2 = "") {
    if (snprintf(java_path, sizeof(java_path), "%s%s%s/" JAVA_EXE, path, path1, path2) >= sizeof(java_path)) {
        return false;
    }

    DWORD attr = GetFileAttributesA(java_path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static BOOL WINAPI console_handler(DWORD dwCtrlType) {
    return dwCtrlType == CTRL_C_EVENT ? TRUE : FALSE;
}

static void execv(const char* app, char* cmd) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcess(app, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        DWORD exit_code = 0;
        WaitForSingleObject(pi.hProcess, INFINITE);
        GetExitCodeProcess(pi.hProcess, &exit_code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ExitProcess(exit_code);
    }
}

static void run_java(char* cmd) {
    SetConsoleCtrlHandler(console_handler, TRUE);

    // 1. Get java executable from JAVA_HOME
    char* java_home = getenv("JAVA_HOME");
    if (java_home != NULL && find_java_at(java_home, "/bin")) {
        execv(java_path, cmd);
    }

    // 2. Try to find java in PATH
    execv(NULL, cmd);

    // 3. Look for java in the common directory
    WIN32_FIND_DATA entry;
    HANDLE dir = FindFirstFile(COMMON_JVM_DIR, &entry);
    if (dir != INVALID_HANDLE_VALUE) {
        do {
            if (entry.cFileName[0] != '.' && (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (find_java_at(COMMON_JVM_DIR, entry.cFileName, "/bin")) {
                    execv(java_path, cmd);
                }
            }
        } while (FindNextFile(dir, &entry));
        CloseHandle(dir);
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

    char* cmd = build_cmdline(argc - 1, argv + 1);
    run_java(cmd);

    // May reach here only if run_java() fails
    fprintf(stderr, "No JDK found. Set JAVA_HOME or ensure java executable is on the PATH\n");
    return 1;
}
