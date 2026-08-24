set(WMS_DEPENDENCY_ROOT "${PROJECT_SOURCE_DIR}/build/dependencies/wms-rc4" CACHE PATH
    "Project-local Stage 1 WMS dependency directory")
set(WMS_SDK_PACKAGE_ROOT "${WMS_DEPENDENCY_ROOT}/package")
set(CPPWINRT_PACKAGE_ROOT "${WMS_DEPENDENCY_ROOT}/cppwinrt-package")
set(CPPWINRT_TOOL "${CPPWINRT_PACKAGE_ROOT}/bin/cppwinrt.exe")
set(WMS_WINMD "${WMS_SDK_PACKAGE_ROOT}/ref/native/Microsoft.Windows.Devices.Midi2.winmd")
set(WMS_GENERATED_ROOT "${CMAKE_CURRENT_BINARY_DIR}/generated")

foreach(required_file IN ITEMS "${CPPWINRT_TOOL}" "${WMS_WINMD}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR
            "Missing pinned Stage 1 dependency: ${required_file}. "
            "Run tools/AcquireStage1Dependencies.ps1 first.")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WMS_GENERATED_ROOT}")
execute_process(
    COMMAND "${CPPWINRT_TOOL}" -input 10.0.26100.0 -output "${WMS_GENERATED_ROOT}" -overwrite
    RESULT_VARIABLE cppwinrt_sdk_result
    OUTPUT_VARIABLE cppwinrt_sdk_output
    ERROR_VARIABLE cppwinrt_sdk_error)
if(NOT cppwinrt_sdk_result EQUAL 0)
    message(FATAL_ERROR "C++/WinRT Windows SDK projection failed: ${cppwinrt_sdk_error}")
endif()
execute_process(
    COMMAND "${CPPWINRT_TOOL}" -input "${WMS_WINMD}" -reference 10.0.26100.0
            -output "${WMS_GENERATED_ROOT}" -overwrite
    RESULT_VARIABLE cppwinrt_wms_result
    OUTPUT_VARIABLE cppwinrt_wms_output
    ERROR_VARIABLE cppwinrt_wms_error)
if(NOT cppwinrt_wms_result EQUAL 0)
    message(FATAL_ERROR "C++/WinRT WMS projection failed: ${cppwinrt_wms_error}")
endif()

add_executable(taureon_wms_spike wms_spike_main.cpp)
target_include_directories(taureon_wms_spike PRIVATE
    "${WMS_GENERATED_ROOT}"
    "${WMS_SDK_PACKAGE_ROOT}/build/native/include"
    "${PROJECT_SOURCE_DIR}/spikes")
target_link_libraries(taureon_wms_spike PRIVATE
    OneCoreUap.lib
    ole32.lib
    runtimeobject.lib
    "${CPPWINRT_PACKAGE_ROOT}/build/native/lib/x64/cppwinrt_fast_forwarder.lib")
target_compile_definitions(taureon_wms_spike PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(taureon_wms_spike PRIVATE /W4 /permissive- /bigobj /EHsc)

add_executable(taureon_wms_winmm_correlation wms_winmm_correlation.cpp)
target_include_directories(taureon_wms_winmm_correlation PRIVATE
    "${WMS_GENERATED_ROOT}"
    "${WMS_SDK_PACKAGE_ROOT}/build/native/include")
target_link_libraries(taureon_wms_winmm_correlation PRIVATE
    OneCoreUap.lib
    ole32.lib
    runtimeobject.lib
    "${CPPWINRT_PACKAGE_ROOT}/build/native/lib/x64/cppwinrt_fast_forwarder.lib")
target_compile_definitions(taureon_wms_winmm_correlation PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(taureon_wms_winmm_correlation PRIVATE /W4 /permissive- /bigobj /EHsc)

if(BUILD_TESTING)
    add_test(NAME stage1_wms_identity_logic COMMAND taureon_wms_spike --identity-only)
    set_tests_properties(stage1_wms_identity_logic PROPERTIES LABELS "stage1;wms")
endif()

find_package(Qt6 6.10 COMPONENTS Core QUIET)
if(Qt6Core_FOUND)
    qt_standard_project_setup()
    add_executable(taureon_wms_qt_harness qcore_harness.cpp wms_worker.cpp wms_worker.hpp)
    set_target_properties(taureon_wms_qt_harness PROPERTIES AUTOMOC ON AUTOUIC ON)
    target_include_directories(taureon_wms_qt_harness PRIVATE
        "${WMS_GENERATED_ROOT}"
        "${WMS_SDK_PACKAGE_ROOT}/build/native/include"
        "${PROJECT_SOURCE_DIR}/spikes")
    target_link_libraries(taureon_wms_qt_harness PRIVATE
        Qt6::Core
        OneCoreUap.lib
        ole32.lib
        runtimeobject.lib
        "${CPPWINRT_PACKAGE_ROOT}/build/native/lib/x64/cppwinrt_fast_forwarder.lib")
    target_compile_definitions(taureon_wms_qt_harness PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(taureon_wms_qt_harness PRIVATE /W4 /permissive- /bigobj /EHsc)
    if(BUILD_TESTING)
        add_test(NAME stage1_qt_wms_start_stop
            COMMAND taureon_wms_qt_harness --auto-test)
        add_test(NAME stage1_qt_wms_close_active
            COMMAND taureon_wms_qt_harness --auto-close-active)
        set_tests_properties(stage1_qt_wms_start_stop stage1_qt_wms_close_active PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:C:/Qt/6.10.3/msvc2022_64/bin"
            TIMEOUT 15
            LABELS "stage1;wms;qt")
    endif()
endif()
