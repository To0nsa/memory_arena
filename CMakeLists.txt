# Minimum CMake version required
cmake_minimum_required(VERSION 3.16)

# Define the project
project(memory_arena VERSION 1.0.0 LANGUAGES C)

# C standard and basic compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_compile_options(-Wall -Wextra -Werror) # Strict warnings for better code quality

# Load sanitizer and debug options from a custom module
include(cmake/SanitizerConfig.cmake)

# Collect all source files recursively
file(GLOB_RECURSE ARENA_SOURCES CONFIGURE_DEPENDS srcs/*.c)

# ───── Library Targets (Static + Shared) ─────

# Define static and shared versions of the library
add_library(memory_arena_static STATIC ${ARENA_SOURCES})
add_library(memory_arena_shared SHARED ${ARENA_SOURCES})
set_target_properties(memory_arena_shared PROPERTIES OUTPUT_NAME memory_arena)

# Define namespace aliases for users
add_library(memory_arena::static ALIAS memory_arena_static)
add_library(memory_arena::shared ALIAS memory_arena_shared)

# Common include paths and feature macros for both libraries
foreach(lib memory_arena_static memory_arena_shared)
    target_include_directories(${lib}
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/internal
    )
    target_compile_definitions(${lib} PUBLIC
        $<$<BOOL:${ARENA_ENABLE_THREAD_SAFE}>:ARENA_ENABLE_THREAD_SAFE>
        $<$<BOOL:${ARENA_POISON_MEMORY}>:ARENA_POISON_MEMORY>
        $<$<BOOL:${ARENA_DEBUG_CHECKS}>:ARENA_DEBUG_CHECKS>
        $<$<BOOL:${ARENA_DEBUG_LOG}>:ARENA_DEBUG_LOG>
    )
endforeach()

# ───── External Dependencies ─────

# Use Notcurses (required by visualizer demo)
find_package(PkgConfig REQUIRED)
pkg_check_modules(NOTCURSES REQUIRED IMPORTED_TARGET notcurses)

# ───── Executable Targets ─────

# Visual demo using the arena visualizer
add_executable(visualizer demo/visualizer.c)
target_link_libraries(visualizer PRIVATE memory_arena_static PkgConfig::NOTCURSES pthread)

# Performance benchmark
add_executable(benchmark benchmark/benchmark_arena_suite.c)
target_link_libraries(benchmark PRIVATE memory_arena_static pthread)

# ───── Tests ─────

# Add testing support (user must enable BUILD_TESTING manually if desired)
include(CTest)
if(BUILD_TESTING)
    message(STATUS "✅ Building test suite")
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.c)
    foreach(test_src ${TEST_SOURCES})
        get_filename_component(test_name ${test_src} NAME_WE)
        add_executable(${test_name} ${test_src})
        target_link_libraries(${test_name} PRIVATE memory_arena_static pthread)
        add_test(NAME ${test_name} COMMAND ${test_name})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
    endforeach()
endif()

# ───── Install Targets ─────

include(GNUInstallDirs)

# Install both static and shared libraries
install(TARGETS memory_arena_static memory_arena_shared
    EXPORT memory_arenaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install public headers (exclude internal ones)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
    PATTERN "internal" EXCLUDE
)

# Install CMake export config for find_package()
install(EXPORT memory_arenaTargets
    FILE memory_arenaTargets.cmake
    NAMESPACE memory_arena::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memory_arena
)

# Custom project configuration file
install(FILES cmake/memory_arenaConfig.cmake.in
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memory_arena
    RENAME memory_arenaConfig.cmake
)

# Versioning info for CMake
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/memory_arenaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/memory_arenaConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/memory_arena
)

# ───── Pkg-config support ─────

# Generate the .pc file from a template
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/memory_arena.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/memory_arena.pc
    @ONLY
)

# Install the generated pkg-config file
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/memory_arena.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)
