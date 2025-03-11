# SPDX-License-Identifier: MIT
### Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved. ###

cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(
    nexus
    VERSION       0.1.0
    DESCRIPTION   "Mapping packets to code"
    HOMEPAGE_URL  "https://github.com/AARInternal/nexus"
    LANGUAGES     CXX C
)

# CMake modules
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake/Modules)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# helper functions
include(cmake/NexusCompilerOptions.cmake)

# disable in-source builds to avoid source tree corruption
if ("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
    message(FATAL_ERROR
        "In-source builds are not supported. Please create a directory for build files and delete CMakeCache.txt and \
        the CMakefiles directory."
    )
endif()

# check if it is built as part of a different project
string(COMPARE EQUAL "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}" is_top_level)
if (NOT "${is_top_level}")
    message(STATUS "Top-level project is ${CMAKE_PROJECT_NAME}")
endif ()

# default build type
set(DEFAULT_BUILD_TYPE "Release")
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to default '${DEFAULT_BUILD_TYPE}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
  # possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif ()

# target
add_library(nexus SHARED)
add_library(nexus::nexus ALIAS nexus)

target_include_directories(nexus
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
        "$<BUILD_INTERFACE:/tmp>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)


set(CMAKE_CXX_FLAGS "-O0 -g")
set(CMAKE_C_FLAGS "-O0 -g")


# library properties
set_target_properties(nexus
    PROPERTIES
        CXX_STANDARD                23
        CXX_STANDARD_REQUIRED       ON
        CXX_EXTENSIONS              OFF
        CXX_VISIBILITY_PRESET       hidden
        VISIBILITY_INLINES_HIDDEN   ON
        POSITION_INDEPENDENT_CODE   ON
        VERSION                     "${PROJECT_VERSION}"
        SOVERSION                   "${PROJECT_VERSION_MAJOR}"
        EXPORT_NAME                 nexus
        OUTPUT_NAME                 nexus
        PUBLIC_HEADER               "${PUBLIC_HEADERS}"
        # enable RPATH during both build and installation; use relative RPATH as well
        SKIP_BUILD_RPATH            FALSE
        BUILD_WITH_INSTALL_RPATH    FALSE
        INSTALL_RPATH_USE_LINK_PATH TRUE
        BUILD_RPATH                 "\$ORIGIN"
        INSTALL_RPATH               "\$ORIGIN"
        # organize lib / bin in the build directory
        ARCHIVE_OUTPUT_DIRECTORY    ${PROJECT_BINARY_DIR}/lib
        LIBRARY_OUTPUT_DIRECTORY    ${PROJECT_BINARY_DIR}/lib
        RUNTIME_OUTPUT_DIRECTORY    ${PROJECT_BINARY_DIR}/bin
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# sources
add_subdirectory(src)
