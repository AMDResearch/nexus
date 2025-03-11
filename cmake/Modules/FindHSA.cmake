# FindHSA.cmake - Locate the HSA runtime library and set up the target

# Search for the HSA include directory with priority given to /opt/rocm

file(GLOB ROCM_PATHS "/opt/rocm*/include")

find_path(HSA_INCLUDE_DIR
    NAMES hsa/hsa.h
    PATHS ${ROCM_PATHS}
    /usr/local/include
    /usr/include
    DOC "Path to HSA include directory"
)

# Search for the HSA library with priority given to /opt/rocm
find_library(HSA_LIBRARY
    NAMES hsa-runtime64
    PATHS /opt/rocm/lib
    /opt/rocm/lib64
    /usr/local/lib
    /usr/lib
    /usr/lib/x86_64-linux-gnu
    DOC "Path to HSA runtime library"
)

message("HSA_INCLUDE_DIR: ${HSA_INCLUDE_DIR}")
message("HSA_LIBRARY: ${HSA_LIBRARY}")

# Check if both the include directory and library were found
if(HSA_INCLUDE_DIR AND HSA_LIBRARY)
    # Create the imported target hsa::hsa
    add_library(hsa::hsa INTERFACE IMPORTED)
    set_target_properties(hsa::hsa PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${HSA_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${HSA_LIBRARY}"
        INTERFACE_COMPILE_DEFINITIONS "AMD_INTERNAL_BUILD"
    )

    # Print status messages
    message(STATUS "HSA include directory: ${HSA_INCLUDE_DIR}")
    message(STATUS "HSA library: ${HSA_LIBRARY}")
    set(HSA_FOUND TRUE)
else()
    # Handle errors if HSA is not found
    if(NOT HSA_INCLUDE_DIR)
        message(WARNING "HSA include directory not found.")
    endif()

    if(NOT HSA_LIBRARY)
        message(WARNING "HSA library not found.")
    endif()

    set(HSA_FOUND FALSE)
endif()

# Provide a variable to indicate whether HSA was found
mark_as_advanced(HSA_INCLUDE_DIR HSA_LIBRARY)
