cmake_minimum_required(VERSION 3.20)
project(cropsim VERSION 0.1.0 LANGUAGES CXX)

option(CROPSIM_BUILD_TESTS "Build cropsim tests" ON)

find_package(yaml-cpp REQUIRED)

add_library(cropsim
    src/renderer.cpp
    src/snapshot.cpp
    src/world.cpp
    src/yaml.cpp
)
add_library(cropsim::cropsim ALIAS cropsim)

target_compile_features(cropsim PUBLIC cxx_std_17)
target_include_directories(cropsim
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_link_libraries(cropsim PRIVATE yaml-cpp::yaml-cpp)
target_compile_options(cropsim PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)

if(CROPSIM_BUILD_TESTS)
    enable_testing()
    find_package(doctest REQUIRED)
    add_executable(cropsim_tests tests/world_tests.cpp)
    target_link_libraries(cropsim_tests PRIVATE cropsim::cropsim doctest::doctest)
    target_compile_options(cropsim_tests PRIVATE -Wall -Wextra -Wpedantic)
    include(${doctest_DIR}/doctest.cmake)
    doctest_discover_tests(cropsim_tests)
endif()
