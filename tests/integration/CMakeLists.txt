add_executable(stage2_winmm_local Stage2WinmmLocal.cpp)
target_link_libraries(stage2_winmm_local PRIVATE taureon_winmm_transport)
target_compile_definitions(stage2_winmm_local PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(stage2_winmm_local PRIVATE /W4 /permissive- /EHsc)

find_program(TAUREON_PWSH_EXECUTABLE NAMES pwsh powershell REQUIRED)
add_test(NAME stage2_local_winmm_wms_loopback
    COMMAND "${TAUREON_PWSH_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
            -File "${PROJECT_SOURCE_DIR}/tools/RunStage2WinmmWmsLoopback.ps1"
            -Executable "$<TARGET_FILE:stage2_winmm_local>" -Cycles 100)
set_tests_properties(stage2_local_winmm_wms_loopback PROPERTIES
    LABELS "stage2;local-midi;integration;winmm"
    RESOURCE_LOCK "local-midi-environment"
    SKIP_RETURN_CODE 77
    TIMEOUT 120)

if(TARGET taureon_wms_transport)
    add_executable(stage2_wms_local Stage2WmsLocal.cpp)
    target_link_libraries(stage2_wms_local PRIVATE taureon_wms_transport)
    target_compile_definitions(stage2_wms_local PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(stage2_wms_local PRIVATE /W4 /permissive- /EHsc)
    add_test(NAME stage2_local_wms_lifecycle
        COMMAND "${TAUREON_PWSH_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${PROJECT_SOURCE_DIR}/tools/RunStage2WinmmWmsLoopback.ps1"
                -Executable "$<TARGET_FILE:stage2_wms_local>" -Cycles 100)
    set_tests_properties(stage2_local_wms_lifecycle PROPERTIES
        LABELS "stage2;local-midi;integration;wms"
        RESOURCE_LOCK "local-midi-environment"
        SKIP_RETURN_CODE 77
        TIMEOUT 120)
endif()

if(TAUREON_BUILD_STAGE1_SPIKES)
    add_test(NAME stage2_local_stage1_winmm_byte_integrity
        COMMAND "${TAUREON_PWSH_EXECUTABLE}" -NoProfile -ExecutionPolicy Bypass
                -File "${PROJECT_SOURCE_DIR}/tools/RunStage1WinmmWmsLoopback.ps1" -Cycles 100)
    set_tests_properties(stage2_local_stage1_winmm_byte_integrity PROPERTIES
        LABELS "stage2;local-midi;integration;stage1-evidence;byte-integrity"
        RESOURCE_LOCK "local-midi-environment"
        TIMEOUT 180)
endif()
