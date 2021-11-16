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

extern int jattach(int pid, int argc, char** argv);

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("jattach " JATTACH_VERSION " built on " __DATE__ "\n"
               "Copyright 2021 Andrei Pangin\n"
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

    return jattach(pid, argc - 2, argv + 2);
}
