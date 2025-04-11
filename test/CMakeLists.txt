cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

project(Tests LANGUAGES HIP)

set(CMAKE_HIP_STANDARD 17)

set(CMAKE_HIP_FLAGS_RELWITHDEBINFO "-O2 -g -save-temps")

add_compile_options($<$<COMPILE_LANGUAGE:HIP>:-g>
                    $<$<COMPILE_LANGUAGE:HIP>:-save-temps>)

function(add_hip_test target_name hip_file)
  add_executable(${target_name} ${hip_file})
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

set(HIP_TEST_NAMES
  vector_add
  vector_add_template
  vector_add_thrust
  vector_add_inline
)

foreach(name IN LISTS HIP_TEST_NAMES)
  add_hip_test(${name} ${name}.hip)
endforeach()