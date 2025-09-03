/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */
#ifdef __linux__

#include "dwarf.h"
#include "testRunner.hpp"
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define CREATE_ELF_READER(file_name)                                       \
        int fd = open(file_name, O_RDONLY);                                \
        ASSERT_NE(fd, -1);                                                 \
                                                                           \
        struct stat st;                                                    \
        ASSERT_NE(fstat(fd, &st), -1);                                     \
        int size = st.st_size;                                             \
                                                                           \
        void* mapped = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0); \
        ASSERT_NE(mapped, MAP_FAILED);                                     \
        const char* base = (const char*)mapped;                            \
        ElfReader elfReader(fd, mapped, size, base);                        

#define SO_WITH_DEBUG_FRAME_32BIT_DWARF_PATH "build/test/lib/libdwarfdebugframe.so"

class ElfReader {
private:
    int _fd;
    void* _map;
    size_t _size;
    const char* _base;

public:
    ElfReader(int fd, void* map, size_t size, const char* base) {
        _fd = fd;
        _map = map;
        _size = size;
        _base = base;
    }
    
    ~ElfReader() {
        munmap(_map, _size);
        close(_fd);
    }
    
    bool isValid() const { 
        const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)_base;
        return _map != nullptr
            && ehdr->e_ident[EI_MAG0] == ELFMAG0
            && ehdr->e_ident[EI_MAG1] == ELFMAG1
            && ehdr->e_ident[EI_MAG2] == ELFMAG2
            && ehdr->e_ident[EI_MAG3] == ELFMAG3;
    }
    
    const char* base() const { return _base; }
    
    bool findDebugFrame(char** start, char** end) {
        if (!isValid()) return false;
        
        const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)_base;
        const Elf64_Shdr* shdr = (const Elf64_Shdr*)(_base + ehdr->e_shoff);
        const char* shstrtab = _base + shdr[ehdr->e_shstrndx].sh_offset;
        
        for (int i = 0; i < ehdr->e_shnum; i++) {
            const char* name = shstrtab + shdr[i].sh_name;
            if (strcmp(name, ".debug_frame") == 0) {
                *start = (char*)(_base + shdr[i].sh_offset);
                *end = *start + shdr[i].sh_size;
                return true;
            }
        }
        return false;
    }
};        

TEST_CASE(Dwarf_SuccessfulParse, fileReadable(SO_WITH_DEBUG_FRAME_32BIT_DWARF_PATH)) {
    CREATE_ELF_READER(SO_WITH_DEBUG_FRAME_32BIT_DWARF_PATH);
    ASSERT_EQ(elfReader.isValid(), true);
    
    char* debug_start;
    char* debug_end;
    ASSERT_EQ(elfReader.findDebugFrame(&debug_start, &debug_end), true);
    
    DwarfParser parser(SO_WITH_DEBUG_FRAME_32BIT_DWARF_PATH, elfReader.base());
    parser.parseDebugFrame(debug_start, debug_end);
    
    ASSERT_GT(parser.count(), 0);
    ASSERT_NE(parser.table(), nullptr);
}

TEST_CASE(Dwarf_EmptyDebugFrame) {
    const char empty_debug_frame[] = {0}; 
    
    DwarfParser parser("test", nullptr);
    parser.parseDebugFrame(empty_debug_frame, empty_debug_frame + sizeof(empty_debug_frame));
    
    ASSERT_EQ(parser.count(), 0);
}

TEST_CASE(Dwarf_FrameDescriptorValidation, fileReadable(SO_WITH_DEBUG_FRAME_32BIT_DWARF_PATH)) {
    CREATE_ELF_READER(SO_WITH_DEBUG_FRAME_32BIT_DWARF_PATH);
    ASSERT_EQ(elfReader.isValid(), true);
    
    char* debug_start;
    char* debug_end;
    ASSERT_EQ(elfReader.findDebugFrame(&debug_start, &debug_end), true);
    
    DwarfParser parser(SO_WITH_DEBUG_FRAME_32BIT_DWARF_PATH, elfReader.base());
    parser.parseDebugFrame(debug_start, debug_end);
    
    bool has_different_locs = false;
    if (parser.count() > 1) {
        FrameDesc* table = parser.table();
        for (int i = 1; i < parser.count(); i++) {
            // It should be sorted
            ASSERT_GTE(table[i].loc, table[i-1].loc);
            // Location should be reasonable (not 0 for real functions)
            ASSERT_GT(table[i].loc, 0);

            if (table[i].loc != table[0].loc) {
                has_different_locs = true;
            }
        }
        // Check that we have frame descriptors with different locations
        ASSERT_EQ(has_different_locs, true);
    }
}

TEST_CASE(Dwarf_NullPointers) {
    // Test constructor with null pointers
    DwarfParser parser("test", nullptr);
    parser.parseDebugFrame(nullptr, nullptr);
    
    ASSERT_EQ(parser.count(), 0);
}

#endif // __linux__
