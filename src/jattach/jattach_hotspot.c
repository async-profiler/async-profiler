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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "psutil.h"


// Check if remote JVM has already opened socket for Dynamic Attach
static int check_socket(int pid) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/.java_pid%d", tmp_path, pid);

    struct stat stats;
    return stat(path, &stats) == 0 && S_ISSOCK(stats.st_mode) ? 0 : -1;
}

// Check if a file is owned by current user
static uid_t get_file_owner(const char* path) {
    struct stat stats;
    return stat(path, &stats) == 0 ? stats.st_uid : (uid_t)-1;
}

// Force remote JVM to start Attach listener.
// HotSpot will start Attach listener in response to SIGQUIT if it sees .attach_pid file
static int start_attach_mechanism(int pid, int nspid) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "/proc/%d/cwd/.attach_pid%d", nspid, nspid);

    int fd = creat(path, 0660);
    if (fd == -1 || (close(fd) == 0 && get_file_owner(path) != geteuid())) {
        // Some mounted filesystems may change the ownership of the file.
        // JVM will not trust such file, so it's better to remove it and try a different path
        unlink(path);

        // Failed to create attach trigger in current directory. Retry in /tmp
        snprintf(path, sizeof(path), "%s/.attach_pid%d", tmp_path, nspid);
        fd = creat(path, 0660);
        if (fd == -1) {
            return -1;
        }
        close(fd);
    }

    // We have to still use the host namespace pid here for the kill() call
    kill(pid, SIGQUIT);

    // Start with 20 ms sleep and increment delay each iteration. Total timeout is 6000 ms
    struct timespec ts = {0, 20000000};
    int result;
    do {
        nanosleep(&ts, NULL);
        result = check_socket(nspid);
    } while (result != 0 && (ts.tv_nsec += 20000000) < 500000000);

    unlink(path);
    return result;
}

// Connect to UNIX domain socket created by JVM for Dynamic Attach
static int connect_socket(int pid) {
    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        return -1;
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;

    int bytes = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/.java_pid%d", tmp_path, pid);
    if (bytes >= sizeof(addr.sun_path)) {
        addr.sun_path[sizeof(addr.sun_path) - 1] = 0;
    }

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

// Send command with arguments to socket
static int write_command(int fd, int argc, char** argv) {
    // Protocol version
    if (write(fd, "1", 2) <= 0) {
        return -1;
    }

    int i;
    for (i = 0; i < 4; i++) {
        const char* arg = i < argc ? argv[i] : "";
        if (write(fd, arg, strlen(arg) + 1) <= 0) {
            return -1;
        }
    }
    return 0;
}

// Mirror response from remote JVM to stdout
static int read_response(int fd, int argc, char** argv) {
    char buf[8192];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    if (bytes == 0) {
        fprintf(stderr, "Unexpected EOF reading response\n");
        return 1;
    } else if (bytes < 0) {
        perror("Error reading response");
        return 1;
    }

    // First line of response is the command result code
    buf[bytes] = 0;
    int result = atoi(buf);

    // Special treatment of 'load' command
    if (result == 0 && argc > 0 && strcmp(argv[0], "load") == 0) {
        size_t total = bytes;
        while (total < sizeof(buf) - 1 && (bytes = read(fd, buf + total, sizeof(buf) - 1 - total)) > 0) {
            total += (size_t)bytes;
        }
        bytes = total;

        // The second line is the result of 'load' command; since JDK 9 it starts from "return code: "
        buf[bytes] = 0;
        result = atoi(strncmp(buf + 2, "return code: ", 13) == 0 ? buf + 15 : buf + 2);
    }

#ifndef SUPPRESS_OUTPUT
    // Mirror JVM response to stdout
    printf("JVM response code = ");
    do {
        fwrite(buf, 1, bytes, stdout);
        bytes = read(fd, buf, sizeof(buf));
    } while (bytes > 0);
    printf("\n");
#endif

    return result;
}

int jattach_hotspot(int pid, int nspid, int argc, char** argv) {
    if (check_socket(nspid) != 0 && start_attach_mechanism(pid, nspid) != 0) {
        perror("Could not start attach mechanism");
        return 1;
    }

    int fd = connect_socket(nspid);
    if (fd == -1) {
        perror("Could not connect to socket");
        return 1;
    }

#ifndef SUPPRESS_OUTPUT
    printf("Connected to remote JVM\n");
#endif

    if (write_command(fd, argc, argv) != 0) {
        perror("Error writing to socket");
        close(fd);
        return 1;
    }

    int result = read_response(fd, argc, argv);
    close(fd);

    return result;
}
