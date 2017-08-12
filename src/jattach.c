/*
 * Copyright 2016 Andrei Pangin
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define PATH_MAX 1024

// See hotspot/src/os/bsd/vm/os_bsd.cpp
// This must be hard coded because it's the system's temporary
// directory not the java application's temp directory, ala java.io.tmpdir.
#ifdef __APPLE__
// macosx has a secure per-user temporary directory

char temp_path_storage[PATH_MAX];

const char* get_temp_directory() {
    static char *temp_path = NULL;
    if (temp_path == NULL) {
        int pathSize = confstr(_CS_DARWIN_USER_TEMP_DIR, temp_path_storage, PATH_MAX);
        if (pathSize == 0 || pathSize > PATH_MAX) {
            strlcpy(temp_path_storage, "/tmp", sizeof (temp_path_storage));
        }
        temp_path = temp_path_storage;
    }
    return temp_path;
}
#else // __APPLE__

const char* get_temp_directory() {
    return "/tmp";
}
#endif // __APPLE__

// Check if remote JVM has already opened socket for Dynamic Attach
static int check_socket(int pid) {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/.java_pid%d", get_temp_directory(), pid);

    struct stat stats;
    return stat(path, &stats) == 0 && S_ISSOCK(stats.st_mode);
}

// Force remote JVM to start Attach listener.
// HotSpot will start Attach listener in response to SIGQUIT if it sees .attach_pid file
static int start_attach_mechanism(int pid) {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/proc/%d/cwd/.attach_pid%d", pid, pid);
    
    int fd = creat(path, 0660);
    if (fd == -1) {
        snprintf(path, PATH_MAX, "%s/.attach_pid%d", get_temp_directory(), pid);
        fd = creat(path, 0660);
        if (fd == -1) {
            return 0;
        }
    }
    close(fd);
    
    kill(pid, SIGQUIT);
    
    int result;
    struct timespec ts = {0, 100000000};
    int retry = 0;
    do {
        nanosleep(&ts, NULL);
        result = check_socket(pid);
    } while (!result && ++retry < 10);

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
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/.java_pid%d", get_temp_directory(), pid);

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
        return 0;
    }

    int i;
    for (i = 0; i < 4; i++) {
        const char* arg = i < argc ? argv[i] : "";
        if (write(fd, arg, strlen(arg) + 1) <= 0) {
            return 0;
        }
    }
    return 1;
}

// Mirror response from remote JVM to stdout
static void read_response(int fd) {
    char buf[8192];
    ssize_t bytes;
    while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
        fwrite(buf, 1, bytes, stdout);
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: jattach <pid> <cmd> <args> ...\n");
        return 1;
    }
    
    int pid = atoi(argv[1]);
    if (pid == 0) {
        perror("Invalid pid provided");
        return 1;
    }

    // Make write() return EPIPE instead of silent process termination
    signal(SIGPIPE, SIG_IGN);

    if (!check_socket(pid) && !start_attach_mechanism(pid)) {
        perror("Could not start attach mechanism");
        return 1;
    }

    int fd = connect_socket(pid);
    if (fd == -1) {
        perror("Could not connect to socket");
        return 1;
    }
    
    printf("Connected to remote JVM\n");
    if (!write_command(fd, argc - 2, argv + 2)) {
        perror("Error writing to socket");
        close(fd);
        return 1;
    }

    printf("Response code = ");
    read_response(fd);

    printf("\n");
    close(fd);
    
    return 0;
}
