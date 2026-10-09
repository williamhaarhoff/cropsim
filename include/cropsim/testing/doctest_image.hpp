#pragma once

#include "cropsim/testing/image_assertion.hpp"

#include <doctest/doctest.h>

#include <cstdio>
#include <iostream>

#define CROPSIM_CHECK_IMAGE(expected, actual, name, artifact_directory)                 \
    DOCTEST_CHECK(::cropsim::testing::compare_images(                                  \
        (expected), (actual), (name), (artifact_directory), std::cerr,                 \
        ::cropsim::testing::terminal_supports_ansi(stderr)))
