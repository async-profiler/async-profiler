/*
 * Copyright The jattach authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "psutil.h"


extern int is_openj9_process(int pid);
extern int jattach_openj9(int pid, int nspid, int argc, char** argv, int print_output);
extern int jattach_hotspot(int pid, int nspid, int argc, char** argv, int print_output);

int mnt_changed = 0;


__attribute__((visibility("default")))
int jattach(int pid, int argc, char** argv, int print_output) {
    uid_t my_uid = geteuid();
    gid_t my_gid = getegid();
    uid_t target_uid = my_uid;
    gid_t target_gid = my_gid;
    int nspid;
    if (get_process_info(pid, &target_uid, &target_gid, &nspid) < 0) {
        fprintf(stderr, "Process %d not found\n", pid);
        return 1;
    }

    // Container support: switch to the target namespaces.
    // Network and IPC namespaces are essential for OpenJ9 connection.
    enter_ns(pid, "net");
    enter_ns(pid, "ipc");
    mnt_changed = enter_ns(pid, "mnt");

    // In HotSpot, dynamic attach is allowed only for the clients with the same euid/egid.
    // If we are running under root, switch to the required euid/egid automatically.
    if ((my_gid != target_gid && setegid(target_gid) != 0) ||
        (my_uid != target_uid && seteuid(target_uid) != 0)) {
        perror("Failed to change credentials to match the target process");
        return 1;
    }

    get_tmp_path(mnt_changed > 0 ? nspid : pid);

    // Make write() return EPIPE instead of abnormal process termination
    signal(SIGPIPE, SIG_IGN);

    if (is_openj9_process(nspid)) {
        return jattach_openj9(pid, nspid, argc, argv, print_output);
    } else {
        return jattach_hotspot(pid, nspid, argc, argv, print_output);
    }
}

#ifdef JATTACH_VERSION

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("jattach " JATTACH_VERSION " built on " __DATE__ "\n"
               "\n"
               "Usage: jattach <pid> <cmd> [args ...]\n"
               "\n"
               "Commands:\n"
               "    load  threaddump   dumpheap  setflag    properties\n"
               "    jcmd  inspectheap  datadump  printflag  agentProperties\n"
               );
        return 1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0) {
        fprintf(stderr, "%s is not a valid process ID\n", argv[1]);
        return 1;
    }

    return jattach(pid, argc - 2, argv + 2, 1);
}

#endif // JATTACH_VERSION
