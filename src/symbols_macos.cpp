/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef __APPLE__

#include <unordered_set>
#include <dlfcn.h>
#include <fnmatch.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
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
std::vector<const char*> Symbols::_includes;
std::vector<const char*> Symbols::_excludes;
static std::unordered_set<const void*> _parsed_libraries;

// Forward declarations.
static const char* basename_of(const char* path);
static std::string deriveJdkLibRootMacos(const char* libjvm_path);

static bool _eager_libjvm_only = false;

void Symbols::setFilter(const std::vector<const char*>& includes,
                        const std::vector<const char*>& excludes) {
    MutexLocker ml(_parse_lock);
    _includes = includes;
    _excludes = excludes;

    for (const char* g : _excludes) {
        if (fnmatch(g, "libjvm.dylib", 0) == 0) {
            Log::warn("symbols-exclude=%s has no effect: libjvm is required by "
                      "the profiler and is parsed during bootstrap before the "
                      "filter is applied", g);
            break;
        }
    }
}

void Symbols::setEagerParseLibjvmOnly(bool on) {
    _eager_libjvm_only = on;
}

static const char* basename_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash != NULL ? slash + 1 : path;
}

static bool matchesAny(const std::vector<const char*>& globs, const char* basename) {
    for (const char* g : globs) {
        if (fnmatch(g, basename, 0) == 0) {
            return true;
        }
    }
    return false;
}

// Returns the basename of the async-profiler shared library itself (e.g.
// "libasyncProfiler.dylib"), or NULL if it can't be determined. Several
// internal paths (MallocTracer / NativeLockTracer initialization, crash
// handler, etc.) call findLibraryByAddress on a function inside our own
// .dylib and ASSERT the result is non-NULL. So we MUST always parse our own
// library regardless of the user's filter.
//
// We compare by basename rather than full path because dladdr returns the
// path used at dlopen time (often relative), while dyld may report a
// canonical absolute path. A realpath normalization would also work but
// basename is sufficient and cheaper.
static const char* selfLibBasename() {
    static char base[256];
    static bool initialized = false;
    if (!initialized) {
        Dl_info info;
        if (dladdr((const void*)&selfLibBasename, &info) && info.dli_fname != NULL) {
            const char* slash = strrchr(info.dli_fname, '/');
            const char* b = slash != NULL ? slash + 1 : info.dli_fname;
            strncpy(base, b, sizeof(base) - 1);
            base[sizeof(base) - 1] = 0;
        } else {
            base[0] = 0;
        }
        initialized = true;
    }
    return base[0] != 0 ? base : NULL;
}

bool Symbols::shouldParseLibrary(const char* path,
                                 const char* jdk_lib_root,
                                 const char* main_exe) {
    const char* base = basename_of(path);
    bool is_libjvm = strcmp(base, "libjvm.dylib") == 0;
    bool is_main_exe = main_exe != NULL && strcmp(path, main_exe) == 0;
    bool is_pseudo = path[0] == '[';
    const char* self_base = selfLibBasename();
    bool is_self = self_base != NULL && strcmp(base, self_base) == 0;

    if (matchesAny(_excludes, base)) {
        if (is_libjvm) {
            Log::warn("symbols-exclude=%s has no effect: libjvm is required by "
                      "the profiler and is parsed during bootstrap before the "
                      "filter is applied", base);
            return true;
        }
        if (is_self) {
            // Internal code paths (MallocTracer, NativeLockTracer, crash handler)
            // assert that async-profiler's own .dylib is in CodeCache. Honoring
            // this exclude would crash on the next start.
            Log::warn("symbols-exclude=%s has no effect: async-profiler's own "
                      "library is required and cannot be filtered out", base);
            return true;
        }
        if (is_main_exe) {
            Log::warn("symbols-exclude is skipping the main executable %s; "
                      "main/launcher frames will appear as [unknown]", base);
        }
        return false;
    }

    if (is_libjvm || is_main_exe || is_pseudo || is_self) {
        return true;
    }
    if (jdk_lib_root != NULL && jdk_lib_root[0] != 0) {
        size_t root_len = strlen(jdk_lib_root);
        if (strncmp(path, jdk_lib_root, root_len) == 0) {
            return true;
        }
    }

    if (_includes.empty()) {
        return true;
    }
    return matchesAny(_includes, base);
}

void Symbols::parseKernelSymbols(CodeCache* cc) {
}

static std::string deriveJdkLibRootMacos(const char* libjvm_path) {
    const char* last_slash = strrchr(libjvm_path, '/');
    if (last_slash == NULL) return "";
    std::string parent(libjvm_path, last_slash - libjvm_path);
    size_t pos = parent.rfind('/');
    if (pos == std::string::npos) return "";
    return parent.substr(0, pos + 1);
}

void Symbols::parseLibraries(CodeCacheArray* array, bool kernel_symbols) {
    MutexLocker ml(_parse_lock);
    uint32_t images = _dyld_image_count();

    bool filter_active = !_includes.empty() || !_excludes.empty();
    std::string jdk_lib_root;
    char main_exe_buf[PATH_MAX];
    const char* main_exe = NULL;
    if (filter_active) {
        for (uint32_t i = 0; i < images; i++) {
            const char* path = _dyld_get_image_name(i);
            if (path != NULL && strcmp(basename_of(path), "libjvm.dylib") == 0) {
                jdk_lib_root = deriveJdkLibRootMacos(path);
                break;
            }
        }
        uint32_t buf_len = sizeof(main_exe_buf);
        if (_NSGetExecutablePath(main_exe_buf, &buf_len) == 0) {
            char* resolved = realpath(main_exe_buf, NULL);
            if (resolved != NULL) {
                strncpy(main_exe_buf, resolved, sizeof(main_exe_buf) - 1);
                main_exe_buf[sizeof(main_exe_buf) - 1] = 0;
                free(resolved);
            }
            main_exe = main_exe_buf;
        }
    }

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
        if (_eager_libjvm_only && path != NULL &&
                strcmp(basename_of(path), "libjvm.dylib") != 0) {
            _parsed_libraries.erase(image_base);
            continue;
        }
        if (filter_active && path != NULL &&
                !shouldParseLibrary(path, jdk_lib_root.c_str(), main_exe)) {
            // Drop the membership we just inserted so a later filter change can retry.
            _parsed_libraries.erase(image_base);
            continue;
        }

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
