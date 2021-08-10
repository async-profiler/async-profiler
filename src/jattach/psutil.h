/*
 * Copyright 2021 Andrei Pangin
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

#ifndef _PSUTIL_H
#define _PSUTIL_H

#include <sys/types.h>


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

#endif // _PSUTIL_H
