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
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include "psutil.h"


#define MAX_NOTIF_FILES 256
static int notif_lock[MAX_NOTIF_FILES];


// Translate HotSpot command to OpenJ9 equivalent
static void translate_command(char* buf, size_t bufsize, int argc, char** argv) {
    const char* cmd = argv[0];

    if (strcmp(cmd, "load") == 0 && argc >= 2) {
        if (argc > 2 && strcmp(argv[2], "true") == 0) {
            snprintf(buf, bufsize, "ATTACH_LOADAGENTPATH(%s,%s)", argv[1], argc > 3 ? argv[3] : "");
        } else {
            snprintf(buf, bufsize, "ATTACH_LOADAGENT(%s,%s)", argv[1], argc > 3 ? argv[3] : "");
        }

    } else if (strcmp(cmd, "jcmd") == 0) {
        snprintf(buf, bufsize, "ATTACH_DIAGNOSTICS:%s,%s", argc > 1 ? argv[1] : "help", argc > 2 ? argv[2] : "");

    } else if (strcmp(cmd, "threaddump") == 0) {
        snprintf(buf, bufsize, "ATTACH_DIAGNOSTICS:Thread.print,%s", argc > 1 ? argv[1] : "");

    } else if (strcmp(cmd, "dumpheap") == 0) {
        snprintf(buf, bufsize, "ATTACH_DIAGNOSTICS:Dump.heap,%s", argc > 1 ? argv[1] : "");

    } else if (strcmp(cmd, "inspectheap") == 0) {
        snprintf(buf, bufsize, "ATTACH_DIAGNOSTICS:GC.class_histogram,%s", argc > 1 ? argv[1] : "");

    } else if (strcmp(cmd, "datadump") == 0) {
        snprintf(buf, bufsize, "ATTACH_DIAGNOSTICS:Dump.java,%s", argc > 1 ? argv[1] : "");

    } else if (strcmp(cmd, "properties") == 0) {
        strcpy(buf, "ATTACH_GETSYSTEMPROPERTIES");

    } else if (strcmp(cmd, "agentProperties") == 0) {
        strcpy(buf, "ATTACH_GETAGENTPROPERTIES");

    } else {
        snprintf(buf, bufsize, "%s", cmd);
    }

    buf[bufsize - 1] = 0;
}

// Unescape a string and print it on stdout
static void print_unescaped(char* str) {
    char* p = strchr(str, '\n');
    if (p != NULL) {
        *p = 0;
    }

    while ((p = strchr(str, '\\')) != NULL) {
        switch (p[1]) {
            case 0:
                break;
            case 'f':
                *p = '\f';
                break;
            case 'n':
                *p = '\n';
                break;
            case 'r':
                *p = '\r';
                break;
            case 't':
                *p = '\t';
                break;
            default:
                *p = p[1];
        }
        fwrite(str, 1, p - str + 1, stdout);
        str = p + 2;
    }

    fwrite(str, 1, strlen(str), stdout);
    printf("\n");
}

// Send command with arguments to socket
static int write_command(int fd, const char* cmd) {
    size_t len = strlen(cmd) + 1;
    size_t off = 0;
    while (off < len) {
        ssize_t bytes = write(fd, cmd + off, len - off);
        if (bytes <= 0) {
            return -1;
        }
        off += bytes;
    }
    return 0;
}

// Mirror response from remote JVM to stdout
static int read_response(int fd, const char* cmd) {
    size_t size = 8192;
    char* buf = malloc(size);

    size_t off = 0;
    while (buf != NULL) {
        ssize_t bytes = read(fd, buf + off, size - off);
        if (bytes == 0) {
            fprintf(stderr, "Unexpected EOF reading response\n");
            return 1;
        } else if (bytes < 0) {
            perror("Error reading response");
            return 1;
        }

        off += bytes;
        if (buf[off - 1] == 0) {
            break;
        }

        if (off >= size) {
            buf = realloc(buf, size *= 2);
        }
    }

    if (buf == NULL) {
        fprintf(stderr, "Failed to allocate memory for response\n");
        return 1;
    }

    int result = 0;

    if (strncmp(cmd, "ATTACH_LOADAGENT", 16) == 0) {
        if (strncmp(buf, "ATTACH_ACK", 10) != 0) {
            // AgentOnLoad error code comes right after AgentInitializationException
            result = strncmp(buf, "ATTACH_ERR AgentInitializationException", 39) == 0 ? atoi(buf + 39) : -1;
        }
    } else if (strncmp(cmd, "ATTACH_DIAGNOSTICS:", 19) == 0) {
        char* p = strstr(buf, "openj9_diagnostics.string_result=");
        if (p != NULL) {
            // The result of a diagnostic command is encoded in Java Properties format
            print_unescaped(p + 33);
            free(buf);
            return result;
        }
    }

    buf[off - 1] = '\n';
    fwrite(buf, 1, off, stdout);

    free(buf);
    return result;
}

static void detach(int fd) {
    if (write_command(fd, "ATTACH_DETACHED") != 0) {
        return;
    }

    char buf[256];
    ssize_t bytes;
    do {
        bytes = read(fd, buf, sizeof(buf));
    } while (bytes > 0 && buf[bytes - 1] != 0);
}

static void close_with_errno(int fd) {
    int saved_errno = errno;
    close(fd);
    errno = saved_errno;
}

static int acquire_lock(const char* subdir, const char* filename) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/.com_ibm_tools_attach/%s/%s", tmp_path, subdir, filename);

    int lock_fd = open(path, O_WRONLY | O_CREAT, 0666);
    if (lock_fd < 0) {
        return -1;
    }

    if (flock(lock_fd, LOCK_EX) < 0) {
        close_with_errno(lock_fd);
        return -1;
    }

    return lock_fd;
}

static void release_lock(int lock_fd) {
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
}

static int create_attach_socket(int* port) {
    // Try IPv6 socket first, then fall back to IPv4
    int s = socket(AF_INET6, SOCK_STREAM, 0);
    if (s != -1) {
        struct sockaddr_in6 addr = {AF_INET6, 0};
        socklen_t addrlen = sizeof(addr);
        if (bind(s, (struct sockaddr*)&addr, addrlen) == 0 && listen(s, 0) == 0
                && getsockname(s, (struct sockaddr*)&addr, &addrlen) == 0) {
            *port = ntohs(addr.sin6_port);
            return s;
        }
    } else if ((s = socket(AF_INET, SOCK_STREAM, 0)) != -1) {
        struct sockaddr_in addr = {AF_INET, 0};
        socklen_t addrlen = sizeof(addr);
        if (bind(s, (struct sockaddr*)&addr, addrlen) == 0 && listen(s, 0) == 0
                && getsockname(s, (struct sockaddr*)&addr, &addrlen) == 0) {
            *port = ntohs(addr.sin_port);
            return s;
        }
    }

    close_with_errno(s);
    return -1;
}

static void close_attach_socket(int s, int pid) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/.com_ibm_tools_attach/%d/replyInfo", tmp_path, pid);
    unlink(path);

    close(s);
}

static unsigned long long random_key() {
    unsigned long long key = time(NULL) * 0xc6a4a7935bd1e995ULL;

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t r = read(fd, &key, sizeof(key));
        (void)r;
        close(fd);
    }

    return key;
}

static int write_reply_info(int pid, int port, unsigned long long key) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/.com_ibm_tools_attach/%d/replyInfo", tmp_path, pid);

    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return -1;
    }

    int chars = snprintf(path, sizeof(path), "%016llx\n%d\n", key, port);
    ssize_t r = write(fd, path, chars);
    (void)r;
    close(fd);

    return 0;
}

static int notify_semaphore(int value, int notif_count) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/.com_ibm_tools_attach/_notifier", tmp_path);

    key_t sem_key = ftok(path, 0xa1);
    int sem = semget(sem_key, 1, IPC_CREAT | 0666);
    if (sem < 0) {
        return -1;
    }

    struct sembuf op = {0, value, value < 0 ? IPC_NOWAIT : 0};
    while (notif_count-- > 0) {
        semop(sem, &op, 1);
    }

    return 0;
}

static int accept_client(int s, unsigned long long key) {
    struct timeval tv = {5, 0};
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int client = accept(s, NULL, NULL);
    if (client < 0) {
        perror("JVM did not respond");
        return -1;
    }

    char buf[35];
    size_t off = 0;
    while (off < sizeof(buf)) {
        ssize_t bytes = recv(client, buf + off, sizeof(buf) - off, 0);
        if (bytes <= 0) {
            fprintf(stderr, "The JVM connection was prematurely closed\n");
            close(client);
            return -1;
        }
        off += bytes;
    }

    char expected[35];
    snprintf(expected, sizeof(expected), "ATTACH_CONNECTED %016llx ", key);
    if (memcmp(buf, expected, sizeof(expected) - 1) != 0) {
        fprintf(stderr, "Unexpected JVM response\n");
        close(client);
        return -1;
    }

    return client;
}

static int lock_notification_files() {
    int count = 0;

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/.com_ibm_tools_attach", tmp_path);

    DIR* dir = opendir(path);
    if (dir != NULL) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL && count < MAX_NOTIF_FILES) {
            if (entry->d_name[0] >= '1' && entry->d_name[0] <= '9' &&
                (entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN)) {
                notif_lock[count++] = acquire_lock(entry->d_name, "attachNotificationSync");
            }
        }
        closedir(dir);
    }

    return count;
}

static void unlock_notification_files(int count) {
    int i;
    for (i = 0; i < count; i++) {
        if (notif_lock[i] >= 0) {
            release_lock(notif_lock[i]);
        }
    }
}

int is_openj9_process(int pid) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "%s/.com_ibm_tools_attach/%d/attachInfo", tmp_path, pid);

    struct stat stats;
    return stat(path, &stats) == 0;
}

int jattach_openj9(int pid, int nspid, int argc, char** argv) {
    int attach_lock = acquire_lock("", "_attachlock");
    if (attach_lock < 0) {
        perror("Could not acquire attach lock");
        return 1;
    }

    int notif_count = 0;
    int port;
    int s = create_attach_socket(&port);
    if (s < 0) {
        perror("Failed to listen to attach socket");
        goto error;
    }

    unsigned long long key = random_key();
    if (write_reply_info(nspid, port, key) != 0) {
        perror("Could not write replyInfo");
        goto error;
    }

    notif_count = lock_notification_files();
    if (notify_semaphore(1, notif_count) != 0) {
        perror("Could not notify semaphore");
        goto error;
    }

    int fd = accept_client(s, key);
    if (fd < 0) {
        // The error message has been already printed
        goto error;
    }

    close_attach_socket(s, nspid);
    unlock_notification_files(notif_count);
    notify_semaphore(-1, notif_count);
    release_lock(attach_lock);

    printf("Connected to remote JVM\n");

    char cmd[8192];
    translate_command(cmd, sizeof(cmd), argc, argv);

    if (write_command(fd, cmd) != 0) {
        perror("Error writing to socket");
        close(fd);
        return 1;
    }

    int result = read_response(fd, cmd);
    if (result != 1) {
        detach(fd);
    }
    close(fd);

    return result;

error:
    if (s >= 0) {
        close_attach_socket(s, nspid);
    }
    if (notif_count > 0) {
        unlock_notification_files(notif_count);
        notify_semaphore(-1, notif_count);
    }
    release_lock(attach_lock);

    return 1;
}
