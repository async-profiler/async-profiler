/*
 * Copyright The async-profiler authors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "catch_amalgamated.hpp"
#include "arguments.h"

TEST_CASE("Arguments can parse") {
    Arguments args;

    SECTION("wall") {
        SECTION("default") {
            Error error = args.parse("wall");
            REQUIRE(!error);
            REQUIRE(args._wall == 0);
        }
        SECTION("non-default") {
            Error error = args.parse("wall=999");
            REQUIRE(!error);
            REQUIRE(args._wall == 999);
        }
    }
}
