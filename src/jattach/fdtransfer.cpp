/*
 * Copyright 2021 Yonatan Goldschmidt
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "utils.h"
#include "../fdTransfer.h"

static bool enter_pid_and_net(pid_t pid) {
    return enter_ns(pid, "net") == 1 && enter_ns(pid, "pid") == 1;
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s pid\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);
    if (pid == 0) {
        fprintf(stderr, "Invalid pid: '%s'\n", argv[1]);
        return 1;
    }

    // get its nspid prior to moving to its PID namespace.
    pid_t nspid = -1;
    uid_t _target_uid;
    gid_t _target_gid;
    if (!get_process_info(pid, &_target_uid, &_target_gid, &nspid)) {
        fprintf(stderr, "Process %d not found\n", pid);
        return 1;
    }

    if (nspid < 0) {
        nspid = alt_lookup_nspid(pid);
    }

    if (!enter_pid_and_net(pid)) {
        perror("Failed to enter PID / net NS of target process");
        return 1;
    }

    // CLONE_NEWPID affects children only - so we fork here.
    if (0 == fork()) {
        // do the actual work
        if (FdTransfer::connectToTarget(nspid)) {
            _exit(FdTransfer::serveRequests() ? 0 : 1);
        } else {
            _exit(1);
        }
    } else {
        int status;
        if (-1 == wait(&status)) {
            perror("wait");
            return 1;
        } else {
            bool success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
            printf("Done serving%s\n", success ? " successfully" : ", had an error");
            return success ? 0 : 1;
        }
    }
}
