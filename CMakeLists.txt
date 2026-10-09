cmake_minimum_required(VERSION 3.20)
project(cropsim VERSION 0.1.0 LANGUAGES CXX)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

option(CROPSIM_BUILD_TESTS "Build cropsim tests" ON)
option(CROPSIM_BUILD_BENCHMARKS "Build opt-in scalability benchmarks" OFF)
option(CROPSIM_BUILD_VIEWER "Build the optional SDL3/SDL_GPU viewer" OFF)
option(CROPSIM_ENABLE_VIEWER_SMOKE_TEST "Enable the display/GPU viewer smoke test" OFF)

find_package(yaml-cpp REQUIRED)
find_package(Boost REQUIRED)

add_library(cropsim
    src/generators/generation_context.cpp
    src/generators/generator_registry.cpp
    src/generators/crop_generators.cpp
    src/generators/field_set_generators.cpp
    src/generators/geometry.cpp
    src/generators/placement_generators.cpp
    src/generators/row_generators.cpp
    src/generators/field_generators.cpp
    src/expression/expression.cpp
    src/modifiers/modifier_fields.cpp
    src/image_assertion.cpp
    src/renderer.cpp
    src/snapshot.cpp
    src/spatial_index.cpp
    src/terminal_image.cpp
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
target_include_directories(cropsim SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(cropsim PRIVATE yaml-cpp::yaml-cpp)
target_link_libraries(cropsim PRIVATE Boost::headers)
target_compile_options(cropsim PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)

if(CROPSIM_BUILD_TESTS)
    enable_testing()
    find_package(doctest REQUIRED)
    add_executable(cropsim_tests tests/world_tests.cpp tests/terminal_image_tests.cpp
        tests/modifier_field_tests.cpp)
    target_link_libraries(cropsim_tests PRIVATE cropsim::cropsim doctest::doctest)
    target_compile_definitions(cropsim_tests PRIVATE
        CROPSIM_TEST_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/test-artifacts"
    )
    target_compile_options(cropsim_tests PRIVATE -Wall -Wextra -Wpedantic)
    include(${doctest_DIR}/doctest.cmake)
    doctest_discover_tests(cropsim_tests)
endif()

if(CROPSIM_BUILD_BENCHMARKS)
    enable_testing()
    add_executable(cropsim_spatial_benchmark tests/spatial_benchmark.cpp)
    target_link_libraries(cropsim_spatial_benchmark PRIVATE cropsim::cropsim)
    target_compile_options(cropsim_spatial_benchmark PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME cropsim_spatial_benchmark COMMAND cropsim_spatial_benchmark)
    set_tests_properties(cropsim_spatial_benchmark PROPERTIES LABELS benchmark)
endif()

if(CROPSIM_BUILD_VIEWER)
    find_package(SDL3 CONFIG REQUIRED)
    find_package(Vulkan REQUIRED)
    find_program(GLSLC_EXECUTABLE glslc REQUIRED)

    set(CROPSIM_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/viewer-shaders")
    set(CROPSIM_VERTEX_SPV "${CROPSIM_SHADER_DIR}/crop.vert.spv")
    set(CROPSIM_FRAGMENT_SPV "${CROPSIM_SHADER_DIR}/crop.frag.spv")
    add_custom_command(OUTPUT "${CROPSIM_VERTEX_SPV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CROPSIM_SHADER_DIR}"
        COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=vertex
                "${CMAKE_CURRENT_SOURCE_DIR}/src/viewer/crop.vert" -o "${CROPSIM_VERTEX_SPV}"
        DEPENDS src/viewer/crop.vert VERBATIM)
    add_custom_command(OUTPUT "${CROPSIM_FRAGMENT_SPV}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CROPSIM_SHADER_DIR}"
        COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=fragment
                "${CMAKE_CURRENT_SOURCE_DIR}/src/viewer/crop.frag" -o "${CROPSIM_FRAGMENT_SPV}"
        DEPENDS src/viewer/crop.frag VERBATIM)
    add_custom_target(cropsim_viewer_shaders DEPENDS
        "${CROPSIM_VERTEX_SPV}" "${CROPSIM_FRAGMENT_SPV}")

    add_library(cropsim_viewer_cli STATIC src/viewer/cli.cpp)
    target_link_libraries(cropsim_viewer_cli PUBLIC cropsim::cropsim)
    target_include_directories(cropsim_viewer_cli PUBLIC src/viewer)
    target_compile_features(cropsim_viewer_cli PUBLIC cxx_std_17)

    add_executable(cropsim_viewer src/viewer/main.cpp)
    add_dependencies(cropsim_viewer cropsim_viewer_shaders)
    target_link_libraries(cropsim_viewer PRIVATE cropsim_viewer_cli SDL3::SDL3 Vulkan::Vulkan)
    target_compile_options(cropsim_viewer PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    add_custom_command(TARGET cropsim_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CROPSIM_VERTEX_SPV}" "${CROPSIM_FRAGMENT_SPV}"
                "$<TARGET_FILE_DIR:cropsim_viewer>")
    if(CROPSIM_BUILD_TESTS)
        add_executable(cropsim_viewer_tests tests/viewer_cli_tests.cpp)
        target_link_libraries(cropsim_viewer_tests PRIVATE cropsim_viewer_cli doctest::doctest)
        target_compile_definitions(cropsim_viewer_tests PRIVATE
            DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
            CROPSIM_VIEWER_TEST_DIR="${CMAKE_CURRENT_BINARY_DIR}/viewer-test-data")
        add_test(NAME cropsim_viewer_cli COMMAND cropsim_viewer_tests)
        if(CROPSIM_ENABLE_VIEWER_SMOKE_TEST)
            add_test(NAME cropsim_viewer_smoke COMMAND cropsim_viewer
                --yaml "${CMAKE_CURRENT_SOURCE_DIR}/tests/viewer_smoke.yaml" --smoke)
            set_tests_properties(cropsim_viewer_smoke PROPERTIES LABELS "viewer;smoke"
                RUN_SERIAL TRUE TIMEOUT 20)
        endif()
    endif()
endif()
