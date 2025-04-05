cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(Tests LANGUAGES HIP)

set(CMAKE_HIP_STANDARD 17)

add_compile_options($<$<COMPILE_LANGUAGE:HIP>:-g>)

function(add_hip_test target_name hip_file)
  add_executable(${target_name} ${hip_file})
  target_include_directories(${target_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
endfunction()

set(HIP_TEST_NAMES
  vector_add
  vector_add_template
  vector_add_thrust
)

foreach(name IN LISTS HIP_TEST_NAMES)
  add_hip_test(${name} ${name}.hip)
endforeach()
