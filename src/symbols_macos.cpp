/*
 * Copyright 2017 Andrei Pangin
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

#ifdef __APPLE__

#include <mach-o/loader.h>
#include <cstring>
#include <mach-o/nlist.h>
#include "symbols.h"
#include <sys/mman.h>
#include <mach-o/dyld_images.h>
#include <fcntl.h>

// Workaround for newer macosx versions: since 10.12.x _dyld_get_all_image_infos exists, but is not exported
extern "C" const struct dyld_all_image_infos* _dyld_get_all_image_infos();

class MachOParser {
  private:
    NativeCodeCache* _cc;
    const uintptr_t _base;
    const void* _header;

    load_command* findCommand(uint32_t command) {
        mach_header_64* header = (mach_header_64*)_header;
        load_command* result = (load_command*)((uint64_t)header + sizeof(mach_header_64));

        if (command == 0) {
            return result;
        }

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (result->cmd == command) {
                break;
            }

            result = (load_command*)((uint64_t)result + result->cmdsize);
        }
        return result;
    }


    void loadSymbols() {
        load_command* command = findCommand(LC_SYMTAB);
        symtab_command* symtab = (symtab_command*)command;
        nlist_64* symbol_table = (nlist_64*)((char*)_header + symtab->symoff);
        const char* str_table = (char*)_header + symtab->stroff;

        for (uint32_t sym_n = 0; sym_n < symtab->nsyms; ++sym_n) {
            nlist_64 descr = symbol_table[sym_n];
            uint32_t str_offset = descr.n_un.n_strx;
            uintptr_t offset = descr.n_value;
            if (offset != 0) {
                const char* symbol_name = &str_table[str_offset];
                if (symbol_name != NULL && (descr.n_type)) {
                    _cc->add((void*)(_base + offset), sizeof(uintptr_t), symbol_name + 1);
                }
            }
        }

        _cc->sort();
    }

  public:
    MachOParser(NativeCodeCache* cc, uintptr_t base, const char* header) : _cc(cc), _base(base), _header(header) {
    }

    static void parseFile(NativeCodeCache* cc, uintptr_t base, const char* file_name) {
        int fd = open(file_name, O_RDONLY);
        if (fd == -1) {
            return;
        }

        size_t length = (size_t)lseek(fd, 0, SEEK_END);
        void* addr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
        close(fd);

        if (addr != NULL) {
            MachOParser parser(cc, base, (char*)addr);
            parser.loadSymbols();
            munmap(addr, length);
        }
    }
};

void Symbols::parseKernelSymbols(NativeCodeCache* cc) {
}

int Symbols::parseMaps(NativeCodeCache** array, int size) {

    const dyld_all_image_infos* dyld_all_image_infos = _dyld_get_all_image_infos();
    const char* lib_path = NULL;
    uintptr_t loaded_lib_addr = 0;
    for (int i = 0; i < dyld_all_image_infos->infoArrayCount; i++) {
        const char* path = dyld_all_image_infos->infoArray[i].imageFilePath;
        size_t length = strlen(path);
        if (strcmp(path + length - 12, "libjvm.dylib") == 0) {
            lib_path = path;
            loaded_lib_addr = (uintptr_t)dyld_all_image_infos->infoArray[i].imageLoadAddress;
        }
    }

    if (lib_path == NULL) {
        return 0;
    }

    NativeCodeCache* cc = new NativeCodeCache(lib_path);
    MachOParser::parseFile(cc, loaded_lib_addr, lib_path);
    array[0] = cc;
    return 1;
}

#endif // __APPLE__
