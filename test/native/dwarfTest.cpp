/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "dwarf.h"
#include "testRunner.hpp"
#include <elf.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

class ElfReader {
private:
    int _fd;
    void* _mapped;
    size_t _size;
    const char* _base;

public:
    ElfReader(const char* filename) : _fd(-1), _mapped(nullptr), _size(0), _base(nullptr) {
        _fd = open(filename, O_RDONLY);
        if (_fd == -1) return;
        
        struct stat st;
        if (fstat(_fd, &st) == -1) return;
        _size = st.st_size;
        
        _mapped = mmap(nullptr, _size, PROT_READ, MAP_PRIVATE, _fd, 0);
        if (_mapped == MAP_FAILED) {
            _mapped = nullptr;
            return;
        }
        _base = (const char*)_mapped;
    }
    
    ~ElfReader() {
        if (_mapped) munmap(_mapped, _size);
        if (_fd != -1) close(_fd);
    }
    
    bool isValid() const { 
        const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)_base;
        return _mapped != nullptr
            && ehdr->e_ident[EI_MAG0] == ELFMAG0
            && ehdr->e_ident[EI_MAG1] == ELFMAG1
            && ehdr->e_ident[EI_MAG2] == ELFMAG2
            && ehdr->e_ident[EI_MAG3] == ELFMAG3;
    }
    const char* base() const { return _base; }
    
    bool findDebugFrame(const char*& start, const char*& end) {
        if (!isValid()) return false;
        
        const Elf64_Ehdr* ehdr = (const Elf64_Ehdr*)_base;
        const Elf64_Shdr* shdr = (const Elf64_Shdr*)(_base + ehdr->e_shoff);
        const char* shstrtab = _base + shdr[ehdr->e_shstrndx].sh_offset;
        
        for (int i = 0; i < ehdr->e_shnum; i++) {
            const char* name = shstrtab + shdr[i].sh_name;
            if (strcmp(name, ".debug_frame") == 0) {
                start = _base + shdr[i].sh_offset;
                end = start + shdr[i].sh_size;
                return true;
            }
        }
        return false;
    }
};

TEST_CASE(Dwarf_SuccessfulParse, fileReadable("build/test/lib/libdwarfdebugframe.so")) {
    ElfReader elf("build/test/lib/libdwarfdebugframe.so");
    ASSERT_EQ(elf.isValid(), true);
    
    const char* debug_start;
    const char* debug_end;
    ASSERT_EQ(elf.findDebugFrame(debug_start, debug_end), true);
    
    DwarfParser parser("libdwarfdebugframe.so", elf.base(), debug_start, debug_end);
    
    ASSERT_GT(parser.count(), 0);
    ASSERT_NE(parser.table(), nullptr);
}

TEST_CASE(Dwarf_EmptyDebugFrame) {
    const char empty_debug_frame[] = {0}; 
    
    DwarfParser parser("test", nullptr, empty_debug_frame, empty_debug_frame + sizeof(empty_debug_frame));
    
    ASSERT_EQ(parser.count(), 0);
}

TEST_CASE(Dwarf_FrameDescriptorValidation, fileReadable("build/test/lib/libdwarfdebugframe.so")) {
    ElfReader elf("build/test/lib/libdwarfdebugframe.so");
    ASSERT_EQ(elf.isValid(), true);
    
    const char* debug_start;
    const char* debug_end;
    ASSERT_EQ(elf.findDebugFrame(debug_start, debug_end), true);
    
    DwarfParser parser("libdwarfdebugframe.so", elf.base(), debug_start, debug_end);
    
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
    DwarfParser parser("test", nullptr, nullptr, nullptr);
    
    ASSERT_EQ(parser.count(), 0);
}