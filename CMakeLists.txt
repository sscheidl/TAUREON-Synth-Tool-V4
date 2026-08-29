cmake_minimum_required(VERSION 3.25)

project(TAUREON_Synth_Tool_V4 VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

add_executable(taureon_bootstrap src/bootstrap_main.cpp)

if(MSVC)
    target_compile_options(taureon_bootstrap PRIVATE /W4 /permissive-)
else()
    target_compile_options(taureon_bootstrap PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

add_subdirectory(src)

option(TAUREON_BUILD_STAGE1_SPIKES "Build isolated Stage 1 native transport spikes" OFF)
if(TAUREON_BUILD_STAGE1_SPIKES AND WIN32)
    add_subdirectory(spikes/wms_direct)
    add_subdirectory(spikes/winmm_direct)
endif()

if(BUILD_TESTING)
    add_test(NAME taureon_bootstrap_runs COMMAND taureon_bootstrap)
    add_subdirectory(tests)
endif()
