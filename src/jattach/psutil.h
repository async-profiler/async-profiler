/*
 * Copyright The jattach authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PSUTIL_H
#define _PSUTIL_H

#include <sys/types.h>


#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PATH 1024
extern char tmp_path[];

// Gets /tmp path of the specified process, as it can be accessed from the host.
// The obtained path is stored in the global tmp_path buffer.
void get_tmp_path(int pid);

// The reentrant version of get_tmp_path.
// Stores the process-specific temporary path into the provided buffer.
// Returns 0 on success, -1 on failure.
int get_tmp_path_r(int pid, char* buf, size_t bufsize);

// Gets the owner uid/gid of the target process, and also its pid inside the container.
// Returns 0 on success, -1 on failure.
int get_process_info(int pid, uid_t* uid, gid_t* gid, int* nspid);

// Tries to enter the namespace of the target process.
// type of the namespace can be "mnt", "net", "pid", etc.
// Returns 1, if the namespace has been successfully changed,
//         0, if the target process is in the same namespace as the host,
//        -1, if the attempt failed.
int enter_ns(int pid, const char* type);

#ifdef __cplusplus
}
#endif

#endif // _PSUTIL_H
