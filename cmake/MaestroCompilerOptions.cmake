# SPDX-License-Identifier: MIT
### Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved. ###

include_guard()

option(NEXUS_WERROR "Make all warnings into errors." OFF)


function(maestro_compiler_options TARGET)
    set_target_properties(${TARGET}
        PROPERTIES
            CXX_STANDARD                23
            CXX_STANDARD_REQUIRED       ON
            CXX_EXTENSIONS              OFF
            CXX_VISIBILITY_PRESET       hidden
            HIP_STANDARD                23
            HIP_STANDARD_REQUIRED       ON
            HIP_EXTENSIONS              OFF
            VISIBILITY_INLINES_HIDDEN   ON
            POSITION_INDEPENDENT_CODE   ON
    )
endfunction()

function(maestro_compiler_warnings TARGET)
    message("Adding ${TARGET}")
    target_compile_options(${TARGET} INTERFACE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic $<$<BOOL:${NEXUS_WERROR}>:-Werror>>
    )
endfunction()
