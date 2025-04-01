# SPDX-License-Identifier: MIT
### Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved. ###

include(${PROJECT_SOURCE_DIR}/cmake/NexusCompilerOptions.cmake)
message("PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}")
include(FetchContent)

#
# dependencies
#

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog
    GIT_TAG        v1.11.0
)
FetchContent_MakeAvailable(spdlog)

include(${PROJECT_SOURCE_DIR}/cmake/CPM.cmake)
CPMAddPackage("gh:fmtlib/fmt#7.1.3")

CPMAddPackage(
    NAME kernelDB
    GIT_REPOSITORY git@github.com:AARInternal/kerneldb.git
    GIT_TAG 7ce03ec170721e1bc1eab1c59fdf25d623ecf011
)

CPMAddPackage("gh:nlohmann/json@3.11.3")

find_package(HSA REQUIRED)

#
# nexus target
#


set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")
set(CMAKE_HIP_FLAGS_DEBUG "-O0 -ggdb")


target_include_directories(nexus
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include/nexus>"
)


target_sources(nexus
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/log.hpp>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/nexus.hpp>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/nexus.cpp
)

nexus_compiler_warnings(nexus)
nexus_compiler_options(nexus)

target_link_libraries(nexus
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        kernelDB64
        fmt::fmt
        hsa::hsa
    PRIVATE
)
