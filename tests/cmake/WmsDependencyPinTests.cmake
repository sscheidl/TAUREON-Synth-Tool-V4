# These configure-only regressions need no SDK download, compiler or native MIDI.
cmake_minimum_required(VERSION 3.25)
file(TO_CMAKE_PATH "${TAUREON_SOURCE_ROOT}" TAUREON_SOURCE_ROOT)
file(TO_CMAKE_PATH "${TAUREON_TEST_ROOT}" TAUREON_TEST_ROOT)
file(MAKE_DIRECTORY "${TAUREON_TEST_ROOT}")

set(cache_checks [=[
cmake_minimum_required(VERSION 3.25)
set(PROJECT_SOURCE_DIR "@TAUREON_TEST_ROOT@/project")
set(TAUREON_ENABLE_WMS_TRANSPORT OFF)
set(TAUREON_WMS_DEPENDENCY_ROOT "${PROJECT_SOURCE_DIR}/build/dependencies/wms-preview10"
    CACHE PATH "Old default" FORCE)
include("@TAUREON_SOURCE_ROOT@/src/transports/wms/CMakeLists.txt")
if(NOT TAUREON_WMS_DEPENDENCY_ROOT STREQUAL "${PROJECT_SOURCE_DIR}/build/dependencies/wms-preview11")
    message(FATAL_ERROR "The cached Preview-10 default was not migrated")
endif()
string(REPLACE "/" "\\" windows_old_default "${PROJECT_SOURCE_DIR}/build/dependencies/wms-preview10")
set(TAUREON_WMS_DEPENDENCY_ROOT "${windows_old_default}" CACHE PATH "Windows-style default" FORCE)
include("@TAUREON_SOURCE_ROOT@/src/transports/wms/CMakeLists.txt")
if(NOT TAUREON_WMS_DEPENDENCY_ROOT STREQUAL "${PROJECT_SOURCE_DIR}/build/dependencies/wms-preview11")
    message(FATAL_ERROR "The Windows-style cached default was not migrated")
endif()
set(TAUREON_WMS_DEPENDENCY_ROOT "@TAUREON_TEST_ROOT@/custom"
    CACHE PATH "Explicit custom root" FORCE)
include("@TAUREON_SOURCE_ROOT@/src/transports/wms/CMakeLists.txt")
if(NOT TAUREON_WMS_DEPENDENCY_ROOT STREQUAL "@TAUREON_TEST_ROOT@/custom")
    message(FATAL_ERROR "An explicit custom dependency root was overwritten")
endif()
]=])
string(CONFIGURE "${cache_checks}" cache_checks @ONLY)
file(WRITE "${TAUREON_TEST_ROOT}/cache-checks.cmake" "${cache_checks}")
execute_process(COMMAND "${CMAKE_COMMAND}" -P "${TAUREON_TEST_ROOT}/cache-checks.cmake"
    RESULT_VARIABLE cache_result OUTPUT_VARIABLE cache_output ERROR_VARIABLE cache_error)
if(NOT cache_result EQUAL 0)
    message(FATAL_ERROR "Cache migration regression failed: ${cache_output}${cache_error}")
endif()

# A custom root with every required filename but stale/modified WINMD must fail
# before projection generation, rather than passing the former existence checks.
set(stale_root "${TAUREON_TEST_ROOT}/stale-custom")
file(MAKE_DIRECTORY "${stale_root}/package/ref/native"
    "${stale_root}/cppwinrt-package/bin"
    "${stale_root}/cppwinrt-package/build/native/lib/x64")
file(WRITE "${stale_root}/package/ref/native/Windows.Devices.Midi2.winmd" "obsolete metadata")
file(WRITE "${stale_root}/cppwinrt-package/bin/cppwinrt.exe" "not executed")
file(WRITE "${stale_root}/cppwinrt-package/build/native/lib/x64/cppwinrt_fast_forwarder.lib" "not linked")
set(stale_check [=[
set(TAUREON_WMS_DEPENDENCY_ROOT "@stale_root@")
include("@TAUREON_SOURCE_ROOT@/cmake/TaureonWmsSdk.cmake")
taureon_configure_wms_projection(unused)
]=])
string(CONFIGURE "${stale_check}" stale_check @ONLY)
file(WRITE "${TAUREON_TEST_ROOT}/stale-check.cmake" "${stale_check}")
execute_process(COMMAND "${CMAKE_COMMAND}" -P "${TAUREON_TEST_ROOT}/stale-check.cmake"
    RESULT_VARIABLE stale_result OUTPUT_VARIABLE stale_output ERROR_VARIABLE stale_error)
if(stale_result EQUAL 0 OR NOT stale_error MATCHES "WMS WINMD does not match pinned Preview 11")
    message(FATAL_ERROR "Stale metadata was not rejected by the pin check: ${stale_output}${stale_error}")
endif()
message(STATUS "WMS cache migration, custom-root preservation and stale-metadata rejection passed")
