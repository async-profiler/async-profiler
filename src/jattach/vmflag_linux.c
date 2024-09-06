/*
 * Copyright The jattach authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __linux__

#define _GNU_SOURCE

#include <fcntl.h>
#include <elf.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>


#ifdef __LP64__
const unsigned char ELFCLASS_SUPPORTED = ELFCLASS64;
typedef Elf64_Ehdr ElfHeader;
typedef Elf64_Shdr ElfSection;
typedef Elf64_Sym  ElfSymbol;
#else
const unsigned char ELFCLASS_SUPPORTED = ELFCLASS32;
typedef Elf32_Ehdr ElfHeader;
typedef Elf32_Shdr ElfSection;
typedef Elf32_Sym  ElfSymbol;
#endif // __LP64__

const unsigned char ELF_IDENT[] = {0x7f, 'E', 'L', 'F', ELFCLASS_SUPPORTED, ELFDATA2LSB, EV_CURRENT};


typedef struct {
    char* addr;
    char* name;
    int flags;
    int type;
} VMFlag;

typedef struct {
    VMFlag* flags;
    char* num_flags;
    char* EnableDynamicAgentLoading;
} VMSymbols;


// Returns the full path and the base address of libjvm.so in the target process
static char* locate_libjvm(int pid, char* jvm_path) {
    char maps[64];
    snprintf(maps, sizeof(maps), "/proc/%d/maps", pid);
    FILE* f = fopen(maps, "r");
    if (f == NULL) {
        return NULL;
    }

    char* base_addr = NULL;
    char* line = NULL;
    size_t line_len = 0;

    ssize_t n;
    while ((n = getline(&line, &line_len, f)) > 0) {
        // Remove newline
        line[n - 1] = 0;

        if (n >= 11 && strcmp(line + n - 11, "/libjvm.so") == 0) {
            const char* addr = line;
            const char* end = strchr(addr, '-') + 1;
            const char* perm = strchr(end, ' ') + 1;
            const char* offs = strchr(perm, ' ') + 1;
            const char* dev = strchr(offs, ' ') + 1;
            const char* inode = strchr(dev, ' ') + 1;
            const char* file = strchr(inode, ' ');
            while (*file == ' ') file++;

            base_addr = (char*)(strtoul(addr, NULL, 16) - strtoul(offs, NULL, 16));
            strcpy(jvm_path, file);
            break;
        }
    }

    free(line);
    fclose(f);
    return base_addr;
}

static inline char* at(void* base, int offset) {
    return (char*)base + offset;
}

static ElfSection* elf_section_at(ElfHeader* ehdr, int index) {
    return (ElfSection*)at(ehdr, ehdr->e_shoff + index * ehdr->e_shentsize);
}

static ElfSection* elf_find_section(ElfHeader* ehdr, uint32_t type) {
    const char* section_names = at(ehdr, elf_section_at(ehdr, ehdr->e_shstrndx)->sh_offset);

    int i;
    for (i = 0; i < ehdr->e_shnum; i++) {
        ElfSection* section = elf_section_at(ehdr, i);
        if (section->sh_type == type) {
            return section;
        }
    }

    return NULL;
}

// Parses libjvm.so to find certain internal variables
static int read_symbols(const char* file_name, char* base_addr, VMSymbols* vmsym) {
    int fd = open(file_name, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    size_t length = lseek(fd, 0, SEEK_END);
    ElfHeader* ehdr = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (ehdr == MAP_FAILED) {
        return -1;
    }

    if (memcmp(ehdr->e_ident, ELF_IDENT, sizeof(ELF_IDENT)) != 0) {
        munmap(ehdr, length);
        return -1;
    }

    ElfSection* symtab;
    if ((symtab = elf_find_section(ehdr, SHT_SYMTAB)) == NULL &&
        (symtab = elf_find_section(ehdr, SHT_DYNSYM)) == NULL) {
        munmap(ehdr, length);
        return -1;
    }

    ElfSection* strtab = elf_section_at(ehdr, symtab->sh_link);
    const char* strings = at(ehdr, strtab->sh_offset);

    const char* symbols = at(ehdr, symtab->sh_offset);
    const char* symbols_end = symbols + symtab->sh_size;

    for (; symbols < symbols_end; symbols += symtab->sh_entsize) {
        ElfSymbol* sym = (ElfSymbol*)symbols;
        if (sym->st_name != 0 && sym->st_value != 0) {
            const char* name = strings + sym->st_name;
            if (strcmp(name, "_ZN7JVMFlag5flagsE") == 0) {
                vmsym->flags = (VMFlag*)(base_addr + sym->st_value);
            } else if (strcmp(name, "_ZN7JVMFlag8numFlagsE") == 0) {
                vmsym->num_flags = base_addr + sym->st_value;
            } else if (strcmp(name, "EnableDynamicAgentLoading") == 0) {
                vmsym->EnableDynamicAgentLoading = base_addr + sym->st_value;
            }
        }
    }

    munmap(ehdr, length);
    return 0;
}

// Helpers to access memory of the target JVM
static ssize_t vm_read(int pid, void* remote, void* local, size_t size) {
    struct iovec local_iov = {local, size};
    struct iovec remote_iov = {remote, size};
    return process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);
}

static ssize_t vm_write(int pid, void* remote, void* local, size_t size) {
    struct iovec local_iov = {local, size};
    struct iovec remote_iov = {remote, size};
    return process_vm_writev(pid, &local_iov, 1, &remote_iov, 1, 0);
}


int enable_dynamic_agent_loading(int pid) {
    char jvm_path[PATH_MAX];
    char* jvm_base = locate_libjvm(pid, jvm_path);
    if (jvm_base == NULL) {
        return -1;
    }

    VMSymbols vmsym = {NULL};
    if (read_symbols(jvm_path, jvm_base, &vmsym) != 0 || vmsym.EnableDynamicAgentLoading == NULL) {
        return -1;
    }

    int num_flags = 0;
    if (vm_read(pid, vmsym.num_flags, &num_flags, sizeof(num_flags)) <= 0) {
        return -1;
    }

    VMFlag* flags = (VMFlag*)malloc(num_flags * sizeof(VMFlag));
    if (vm_read(pid, vmsym.flags, flags, num_flags * sizeof(VMFlag)) <= 0) {
        free(flags);
        return -1;
    }

    int ret = -1;
    int i;
    for (i = 0; i < num_flags; i++) {
        if (flags[i].addr == vmsym.EnableDynamicAgentLoading) {
            char value = 1;
            int origin = 0x20001;
            if (vm_write(pid, vmsym.EnableDynamicAgentLoading, &value, sizeof(value)) > 0 &&
                vm_write(pid, &vmsym.flags[i].flags, &origin, sizeof(origin)) > 0) {
                ret = 0;
            }
            break;
        }
    }

    free(flags);
    return ret;
}

#else

// Not yet implemented on macOS
int enable_dynamic_agent_loading(int pid) {
    return -1;
}

#endif // __linux__
