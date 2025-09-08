/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __APPLE__

#include <unordered_set>
#include <dlfcn.h>
#include <string.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include "symbols.h"
#include "log.h"

UnloadProtection::UnloadProtection(const CodeCache *cc) {
    // Protect library from unloading while parsing in-memory ELF program headers.
    // Also, dlopen() ensures the library is fully loaded.
    _lib_handle = dlopen(cc->name(), RTLD_LAZY | RTLD_NOLOAD);
    _valid = _lib_handle != NULL;
}

UnloadProtection::~UnloadProtection() {
    if (_lib_handle != NULL) {
        dlclose(_lib_handle);
    }
}

class MachOParser {
  private:
    CodeCache* _cc;
    const mach_header* _image_base;
    const char* _vmaddr_slide;

    static const char* add(const void* base, uint64_t offset) {
        return (const char*)base + offset;
    }

    void findSymbolPtrSection(const segment_command_64* sc, const section_64** section_ptr) {
        const section_64* section = (const section_64*)add(sc, sizeof(segment_command_64));
        for (uint32_t i = 0; i < sc->nsects; i++) {
            uint32_t section_type = section->flags & SECTION_TYPE;
            if (section_type == S_NON_LAZY_SYMBOL_POINTERS) {
                section_ptr[0] = section;
            } else if (section_type == S_LAZY_SYMBOL_POINTERS) {
                section_ptr[1] = section;
            }
            section++;
        }
    }

    const section_64* findSection(const segment_command_64* sc, const char* section_name) {
        const section_64* section = (const section_64*)add(sc, sizeof(segment_command_64));
        for (uint32_t i = 0; i < sc->nsects; i++) {
            if (strcmp(section->sectname, section_name) == 0) {
                return section;
            }
            section++;
        }
        return NULL;
    }

    void loadSymbols(const symtab_command* symtab, const char* link_base) {
        const nlist_64* sym = (const nlist_64*)add(link_base, symtab->symoff);
        const char* str_table = add(link_base, symtab->stroff);
        bool debug_symbols = false;

        for (uint32_t i = 0; i < symtab->nsyms; i++) {
            if ((sym->n_type & 0xee) == 0x0e && sym->n_value != 0) {
                const char* addr = _vmaddr_slide + sym->n_value;
                const char* name = str_table + sym->n_un.n_strx;
                if (name[0] == '_') name++;
                _cc->add(addr, 0, name);
                debug_symbols = true;
            }
            sym++;
        }

        _cc->setDebugSymbols(debug_symbols);
    }

    void loadStubSymbols(const symtab_command* symtab, const dysymtab_command* dysymtab,
                         const section_64* stubs_section, const char* link_base) {
        const nlist_64* sym = (const nlist_64*)add(link_base, symtab->symoff);
        const char* str_table = add(link_base, symtab->stroff);

        const uint32_t* isym = (const uint32_t*)add(link_base, dysymtab->indirectsymoff) + stubs_section->reserved1;
        uint32_t isym_count = stubs_section->size / stubs_section->reserved2;
        const char* stubs_start = _vmaddr_slide + stubs_section->addr;

        for (uint32_t i = 0; i < isym_count; i++) {
            if ((isym[i] & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) == 0) {
                const char* name = str_table + sym[isym[i]].n_un.n_strx;
                if (name[0] == '_') name++;

                char stub_name[256];
                snprintf(stub_name, sizeof(stub_name), "stub:%s", name);
                _cc->add(stubs_start + i * stubs_section->reserved2, stubs_section->reserved2, stub_name);
            }
        }

        _cc->setPlt(stubs_section->addr, isym_count * stubs_section->reserved2);
    }

    void loadImports(const symtab_command* symtab, const dysymtab_command* dysymtab,
                     const section_64* symbol_ptr_section, const char* link_base) {
        const nlist_64* sym = (const nlist_64*)add(link_base, symtab->symoff);
        const char* str_table = add(link_base, symtab->stroff);

        const uint32_t* isym = (const uint32_t*)add(link_base, dysymtab->indirectsymoff) + symbol_ptr_section->reserved1;
        uint32_t isym_count = symbol_ptr_section->size / sizeof(void*);
        void** slot = (void**)(_vmaddr_slide + symbol_ptr_section->addr);

        for (uint32_t i = 0; i < isym_count; i++) {
            if ((isym[i] & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) == 0) {
                const char* name = str_table + sym[isym[i]].n_un.n_strx;
                if (name[0] == '_') name++;
                _cc->addImport(&slot[i], name);
            }
        }
    }

  public:
    MachOParser(CodeCache* cc, const mach_header* image_base, const char* vmaddr_slide) :
        _cc(cc), _image_base(image_base), _vmaddr_slide(vmaddr_slide) {}

    bool parse() {
        if (_image_base->magic != MH_MAGIC_64) {
            return false;
        }

        const mach_header_64* header = (const mach_header_64*)_image_base;
        const load_command* lc = (const load_command*)(header + 1);

        const char* link_base = NULL;
        const section_64* symbol_ptr[2] = {NULL, NULL};
        const symtab_command* symtab = NULL;
        const dysymtab_command* dysymtab = NULL;
        const section_64* stubs_section = NULL;

        for (uint32_t i = 0; i < header->ncmds; i++) {
            if (lc->cmd == LC_SEGMENT_64) {
                const segment_command_64* sc = (const segment_command_64*)lc;
                if (strcmp(sc->segname, "__TEXT") == 0) {
                    _cc->updateBounds(_image_base, add(_image_base, sc->vmsize));
                    stubs_section = findSection(sc, "__stubs");
                } else if (strcmp(sc->segname, "__LINKEDIT") == 0) {
                    link_base = _vmaddr_slide + sc->vmaddr - sc->fileoff;
                } else if (strcmp(sc->segname, "__DATA") == 0 || strcmp(sc->segname, "__DATA_CONST") == 0) {
                    findSymbolPtrSection(sc, symbol_ptr);
                }
            } else if (lc->cmd == LC_SYMTAB) {
                symtab = (const symtab_command*)lc;
            } else if (lc->cmd == LC_DYSYMTAB) {
                dysymtab = (const dysymtab_command*)lc;
            }
            lc = (const load_command*)add(lc, lc->cmdsize);
        }

        if (symtab != NULL && link_base != NULL) {
            loadSymbols(symtab, link_base);

            if (dysymtab != NULL) {
                if (symbol_ptr[0] != NULL) loadImports(symtab, dysymtab, symbol_ptr[0], link_base);
                if (symbol_ptr[1] != NULL) loadImports(symtab, dysymtab, symbol_ptr[1], link_base);
                if (stubs_section != NULL) loadStubSymbols(symtab, dysymtab, stubs_section, link_base);
            }
        }

        return true;
    }
};


Mutex Symbols::_parse_lock;
bool Symbols::_have_kernel_symbols = false;
bool Symbols::_libs_limit_reported = false;
static std::unordered_set<const void*> _parsed_libraries;

void Symbols::parseKernelSymbols(CodeCache* cc) {
}

void Symbols::parseLibraries(CodeCacheArray* array, bool kernel_symbols) {
    MutexLocker ml(_parse_lock);
    uint32_t images = _dyld_image_count();

    for (uint32_t i = 0; i < images; i++) {
        const mach_header* image_base = _dyld_get_image_header(i);
        if (image_base == NULL || !_parsed_libraries.insert(image_base).second) {
            continue;  // the library was already parsed
        }

        int count = array->count();
        if (count >= MAX_NATIVE_LIBS) {
            if (!_libs_limit_reported) {
                Log::warn("Number of parsed libraries reached the limit of %d", MAX_NATIVE_LIBS);
                _libs_limit_reported = true;
            }
            break;
        }

        const char* path = _dyld_get_image_name(i);
        const char* vmaddr_slide = (const char*)_dyld_get_image_vmaddr_slide(i);

        CodeCache* cc = new CodeCache(path, count);
        cc->setTextBase(vmaddr_slide);

        UnloadProtection handle(cc);
        if (handle.isValid()) {
            MachOParser parser(cc, image_base, vmaddr_slide);
            if (!parser.parse()) {
                Log::warn("Could not parse symbols from %s", path);
            }
            cc->sort();
            array->add(cc);
        } else {
            delete cc;
        }
    }
}

#endif // __APPLE__
