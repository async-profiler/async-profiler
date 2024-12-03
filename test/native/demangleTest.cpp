/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "arch.h"
#include "testRunner.hpp"
#include "demangle.h"
#include <stdio.h>

TEST_CASE(Demangle_test_needs_demangling) {
    // Rust legacy-mangled symbol
    CHECK_EQ(Demangle::needsDemangling("_ZN12panic_unwind3imp5panic17exception_cleanup17he4cf772173d90f46E"), true);
    // Rust v0-mangled symbol
    CHECK_EQ(Demangle::needsDemangling("_RNvCs6KtT2fMGqXk_8infiloop4main"), true);
    // C++ symbol
    CHECK_EQ(Demangle::needsDemangling("_ZNKSbIwSt11char_traitsIwESaIwEE4_Rep12_M_is_sharedEv"), true);
    // C symbols
    CHECK_EQ(Demangle::needsDemangling("malloc"), false);
    CHECK_EQ(Demangle::needsDemangling("_malloc"), false);
}

// demangle calls _cxa_demangle, which should support demangling Itanium-mangled symbols across all Linux systems,
// but might have a different mangling format on some other platforms.
#ifdef __linux__
TEST_CASE(Demangle_test_demangle_cpp) {
    const char *s = Demangle::demangle("_ZNSt15basic_streambufIwSt11char_traitsIwEE9pbackfailEj", false);
    CHECK_EQ (strcmp(s, "std::basic_streambuf<wchar_t, std::char_traits<wchar_t> >::pbackfail"), 0);
    free((void*)s);
}

TEST_CASE(Demangle_test_demangle_cpp_rust_like) {
    const char *s = Demangle::demangle("_ZN5MyMapESt6vectorIRKSsE", true);
    CHECK_EQ (strcmp(s, "MyMap(std::vector<std::string const&>)"), 0);
    free((void*)s);
}

TEST_CASE(Demangle_test_demangle_cpp_full_signature) {
    const char *s = Demangle::demangle("_ZNSt15basic_streambufIwSt11char_traitsIwEE9pbackfailEj", true);
    CHECK_EQ (strcmp(s, "std::basic_streambuf<wchar_t, std::char_traits<wchar_t> >::pbackfail(unsigned int)"), 0);
    free((void*)s);
}
#endif

TEST_CASE(Demangle_test_demangle_rust_legacy) {
    const char *s = Demangle::demangle("_ZN12panic_unwind3imp5panic17exception_cleanup17he4cf772173d90f46E", false);
    printf("A0=%s", s);
    CHECK_EQ (strcmp(s, "panic_unwind::imp::panic::exception_cleanup"), 0);
    free((void*)s);
}

TEST_CASE(Demangle_test_demangle_rust_legacy_full_signature) {
    const char *s = Demangle::demangle("_ZN12panic_unwind3imp5panic17exception_cleanup17he4cf772173d90f46E", true);
    CHECK_EQ (strcmp(s, "panic_unwind::imp::panic::exception_cleanup::he4cf772173d90f46"), 0);
    free((void*)s);
}

TEST_CASE(Demangle_test_demangle_rust_v0) {
    const char *s = Demangle::demangle("_RNvCs6KtT2fMGqXk_8infiloop4main", false);
    CHECK_EQ (strcmp(s, "infiloop::main"), 0);
    free((void*)s);
}

TEST_CASE(Demangle_test_demangle_rust_v0_expanding) {
    // Test that demangling of a symbol that is big is handled correctly
    const char *s = Demangle::demangle("_RNvMC0" "TTTTTT" "p" "Ba_E" "B9_E" "B8_E" "B7_E" "B6_E" "B5_E" "3run", false);
    printf("%s\n", s);
    const char *expected =
        "<"
        "((((((_, _), (_, _)), ((_, _), (_, _))), (((_, _), (_, _)), ((_, _), (_, _)))), "
        "((((_, _), (_, _)), ((_, _), (_, _))), (((_, _), (_, _)), ((_, _), (_, _))))), "
        "(((((_, _), (_, _)), ((_, _), (_, _))), (((_, _), (_, _)), ((_, _), (_, _)))), "
        "((((_, _), (_, _)), ((_, _), (_, _))), (((_, _), (_, _)), ((_, _), (_, _))))))"
        ">::run";
    CHECK_EQ (strcmp(s, expected), 0);
    free((void*)s);
}

TEST_CASE(Demangle_test_demangle_rust_v0_infinite) {
    // Test that demangling of a symbol that is stupidly big is handled correctly
    const char *s = Demangle::demangle("_RNvMC0" "TTTTTTTTTT" "p" "Be_E" "Bd_E" "Bc_E" "Bb_E" "Ba_E" "B9_E" "B8_E" "B7_E" "B6_E" "B5_E" "3run", false);
    CHECK_EQ (s, NULL);
}

TEST_CASE(Demangle_test_demangle_rust_v0_full_signature) {
    const char *s = Demangle::demangle("_RNvCs6KtT2fMGqXk_8infiloop4main", true);
    CHECK_EQ (strcmp(s, "infiloop[4e9e38d21762ec98]::main"), 0);
    free((void*)s);
}
