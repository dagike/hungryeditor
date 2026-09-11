cmake_minimum_required(VERSION 3.24)

project(hungryeditor
    VERSION 0.1.0
    DESCRIPTION "A fast native Markdown editor"
    LANGUAGES C CXX)

# --- Standard ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Defaults -------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# --- Options -----------------------------------------------------------------
option(HUNGRYEDITOR_WERROR "Treat compiler warnings as errors" OFF)
# Off by default (and for the ordinary linux-debug/windows-release CI matrix):
# test_benchmarks' thresholds stay the loose, gross-regression-only ones an
# unoptimized, noisy CI VM can meet. The dedicated release-benchmarks CI job
# turns this on to assert the plan's real performance targets instead — see
# tests/test_benchmarks.cpp.
option(HUNGRYEDITOR_STRICT_BENCHMARKS
    "Assert the plan's real performance targets in test_benchmarks, not just a gross-regression guard"
    OFF)
include(CTest)  # defines BUILD_TESTING (ON by default)

include(CompilerWarnings)

# --- Vendored dependencies -----------------------------------------------
add_subdirectory(third_party)

# --- Targets ---------------------------------------------------------------
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# --- Packaging ---------------------------------------------------------------
include(Packaging)

# --- Summary ---------------------------------------------------------------
message(STATUS "hungryeditor ${PROJECT_VERSION}")
message(STATUS "  build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  werror     : ${HUNGRYEDITOR_WERROR}")
message(STATUS "  testing    : ${BUILD_TESTING}")
message(STATUS "  strict benchmarks : ${HUNGRYEDITOR_STRICT_BENCHMARKS}")
