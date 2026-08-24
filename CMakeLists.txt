cmake_minimum_required(VERSION 3.25)

project(TAUREON_Synth_Tool_V4 VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(taureon_bootstrap src/bootstrap_main.cpp)

if(MSVC)
    target_compile_options(taureon_bootstrap PRIVATE /W4 /permissive-)
else()
    target_compile_options(taureon_bootstrap PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME taureon_bootstrap_runs COMMAND taureon_bootstrap)
endif()
