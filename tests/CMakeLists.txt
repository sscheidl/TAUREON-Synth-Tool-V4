add_executable(stage2_core_tests unit/Stage2CoreTests.cpp)
target_include_directories(stage2_core_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage2_core_tests PRIVATE taureon_midi_core)
if(MSVC)
    target_compile_options(stage2_core_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage2_core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage2_core_unit COMMAND stage2_core_tests)
set_tests_properties(stage2_core_unit PROPERTIES LABELS "stage2;unit;ci")

function(taureon_add_stage3_unit target source)
    add_executable(${target} ${source})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(${target} PRIVATE taureon_midi_core)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES LABELS "stage3;unit;ci")
endfunction()

taureon_add_stage3_unit(stage3_midi_sysex_unit unit/Stage3MidiSysexTests.cpp)
taureon_add_stage3_unit(stage3_syx_file_unit unit/Stage3SyxFileTests.cpp)
taureon_add_stage3_unit(stage3_transfer_unit unit/Stage3TransferTests.cpp)
taureon_add_stage3_unit(stage3_large_sysex_unit unit/Stage3LargeSysexTests.cpp)
taureon_add_stage3_unit(stage3_handle_growth_unit unit/Stage3HandleGrowthTests.cpp)
set_tests_properties(stage3_large_sysex_unit PROPERTIES LABELS "stage3;unit;ci;large-data")

add_executable(stage4_profile_tests unit/Stage4ProfileTests.cpp)
target_include_directories(stage4_profile_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage4_profile_tests PRIVATE taureon_midi_core)
target_compile_definitions(stage4_profile_tests PRIVATE TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(stage4_profile_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage4_profile_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage4_profile_unit COMMAND stage4_profile_tests)
set_tests_properties(stage4_profile_unit PROPERTIES LABELS "stage4;unit;ci")

find_package(Qt6 6.10 REQUIRED COMPONENTS Core Widgets)
add_test(NAME stage5_gui_smoke COMMAND taureon_app --smoke-test)
set_tests_properties(stage5_gui_smoke PROPERTIES
    LABELS "stage5;gui;ci"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stage5_application_tests
    unit/Stage5ApplicationTests.cpp
    "${PROJECT_SOURCE_DIR}/src/gui/MidiMonitorFilterModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/MonitorEventBridge.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/MidiMonitorModel.cpp")
target_include_directories(stage5_application_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_application_tests PRIVATE taureon_midi_core Qt6::Core)
if(MSVC)
    target_compile_options(stage5_application_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_application_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_application_unit COMMAND stage5_application_tests)
set_tests_properties(stage5_application_unit PROPERTIES
    LABELS "stage5;unit;ci"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stage5_profile_followup_tests unit/Stage5ProfileFollowupTests.cpp)
target_include_directories(stage5_profile_followup_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_profile_followup_tests PRIVATE taureon_midi_core)
target_compile_definitions(stage5_profile_followup_tests PRIVATE TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(stage5_profile_followup_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_profile_followup_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_profile_followup_unit COMMAND stage5_profile_followup_tests)
set_tests_properties(stage5_profile_followup_unit PROPERTIES LABELS "stage5;unit;ci")

add_executable(stage5_profile_ui_tests
    unit/Stage5ProfileUiTests.cpp
    "${PROJECT_SOURCE_DIR}/src/gui/ProfileMatchPanel.cpp")
target_include_directories(stage5_profile_ui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_profile_ui_tests PRIVATE taureon_midi_core Qt6::Widgets)
target_compile_definitions(stage5_profile_ui_tests PRIVATE TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(stage5_profile_ui_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_profile_ui_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_profile_ui_unit COMMAND stage5_profile_ui_tests)
set_tests_properties(stage5_profile_ui_unit PROPERTIES
    LABELS "stage5;gui;unit;ci"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stage5_monitor_stress_tests
    unit/Stage5MonitorStressTests.cpp
    "${PROJECT_SOURCE_DIR}/src/gui/MonitorEventBridge.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/MidiMonitorModel.cpp")
target_include_directories(stage5_monitor_stress_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_monitor_stress_tests PRIVATE taureon_midi_core Qt6::Core)
if(MSVC)
    target_compile_options(stage5_monitor_stress_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_monitor_stress_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_monitor_stress COMMAND stage5_monitor_stress_tests)
set_tests_properties(stage5_monitor_stress PROPERTIES
    LABELS "stage5;stress;ci"
    TIMEOUT 30
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stage5_connection_worker_tests
    unit/Stage5ConnectionWorkerTests.cpp
    "${PROJECT_SOURCE_DIR}/src/app/ConnectionWorker.cpp")
target_include_directories(stage5_connection_worker_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_connection_worker_tests PRIVATE taureon_midi_core)
target_compile_definitions(stage5_connection_worker_tests PRIVATE
    TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(stage5_connection_worker_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_connection_worker_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_connection_worker_unit COMMAND stage5_connection_worker_tests)
set_tests_properties(stage5_connection_worker_unit PROPERTIES LABELS "stage5;unit;ci")

add_executable(stage5_connection_ui_tests
    unit/Stage5ConnectionUiTests.cpp
    "${PROJECT_SOURCE_DIR}/src/app/ConnectionWorker.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/MainWindow.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/MidiMonitorFilterModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/MonitorEventBridge.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/MidiMonitorModel.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/ProfileMatchPanel.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/SysExTransferPanel.cpp"
    "${PROJECT_SOURCE_DIR}/src/gui/SysExManagerPanel.cpp")
set_target_properties(stage5_connection_ui_tests PROPERTIES AUTOMOC ON)
target_include_directories(stage5_connection_ui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_connection_ui_tests PRIVATE taureon_midi_core Qt6::Widgets)
target_compile_definitions(stage5_connection_ui_tests PRIVATE
    TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(stage5_connection_ui_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_connection_ui_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_connection_ui_unit COMMAND stage5_connection_ui_tests)
set_tests_properties(stage5_connection_ui_unit PROPERTIES
    LABELS "stage5;gui;unit;ci"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")

add_executable(stage5_sysex_transfer_session_tests unit/Stage5SysExTransferSessionTests.cpp)
target_include_directories(stage5_sysex_transfer_session_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_sysex_transfer_session_tests PRIVATE taureon_midi_core)
target_compile_definitions(stage5_sysex_transfer_session_tests PRIVATE
    TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    TAUREON_TEST_OUTPUT_DIR="$<TARGET_FILE_DIR:stage5_sysex_transfer_session_tests>")
if(MSVC)
    target_compile_options(stage5_sysex_transfer_session_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_sysex_transfer_session_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_sysex_transfer_session_unit COMMAND stage5_sysex_transfer_session_tests)
set_tests_properties(stage5_sysex_transfer_session_unit PROPERTIES LABELS "stage5;unit;ci")


add_executable(stage5_sysex_manager_tests unit/Stage5SysExManagerTests.cpp)
target_include_directories(stage5_sysex_manager_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_sysex_manager_tests PRIVATE taureon_midi_core)
target_compile_definitions(stage5_sysex_manager_tests PRIVATE
    TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
    TAUREON_TEST_OUTPUT_DIR="$<TARGET_FILE_DIR:stage5_sysex_manager_tests>")
if(MSVC)
    target_compile_options(stage5_sysex_manager_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_sysex_manager_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_sysex_manager_unit COMMAND stage5_sysex_manager_tests)
set_tests_properties(stage5_sysex_manager_unit PROPERTIES LABELS "stage5;unit;ci")

add_executable(stage5_sysex_manager_ui_tests
    unit/Stage5SysExManagerUiTests.cpp
    "${PROJECT_SOURCE_DIR}/src/gui/SysExManagerPanel.cpp")
set_target_properties(stage5_sysex_manager_ui_tests PROPERTIES AUTOMOC ON)
target_include_directories(stage5_sysex_manager_ui_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_sysex_manager_ui_tests PRIVATE taureon_midi_core Qt6::Widgets)
target_compile_definitions(stage5_sysex_manager_ui_tests PRIVATE TAUREON_SOURCE_DIR="${PROJECT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(stage5_sysex_manager_ui_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_sysex_manager_ui_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_sysex_manager_ui_unit COMMAND stage5_sysex_manager_ui_tests)
set_tests_properties(stage5_sysex_manager_ui_unit PROPERTIES
    LABELS "stage5;gui;unit;ci"
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")


add_executable(stage5_settings_diagnostics_tests unit/Stage5SettingsDiagnosticsTests.cpp)
target_include_directories(stage5_settings_diagnostics_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(stage5_settings_diagnostics_tests PRIVATE taureon_midi_core)
target_compile_definitions(stage5_settings_diagnostics_tests PRIVATE
    TAUREON_TEST_OUTPUT_DIR="$<TARGET_FILE_DIR:stage5_settings_diagnostics_tests>")
if(MSVC)
    target_compile_options(stage5_settings_diagnostics_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(stage5_settings_diagnostics_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME stage5_settings_diagnostics_unit COMMAND stage5_settings_diagnostics_tests)
set_tests_properties(stage5_settings_diagnostics_unit PROPERTIES LABELS "stage5;unit;ci")

if(WIN32)
    add_executable(stage2_winmm_header_tests unit/Stage2WinmmHeaderTests.cpp)
    target_include_directories(stage2_winmm_header_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(stage2_winmm_header_tests PRIVATE taureon_winmm_transport)
    target_compile_definitions(stage2_winmm_header_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(stage2_winmm_header_tests PRIVATE /W4 /permissive- /EHsc)
    add_test(NAME stage2_winmm_header_unit COMMAND stage2_winmm_header_tests)
    set_tests_properties(stage2_winmm_header_unit PROPERTIES LABELS "stage2;unit;ci;winmm")

    add_executable(stage2_winmm_transport_tests unit/Stage2WinmmTransportTests.cpp)
    target_include_directories(stage2_winmm_transport_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(stage2_winmm_transport_tests PRIVATE taureon_winmm_transport)
    target_compile_definitions(stage2_winmm_transport_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(stage2_winmm_transport_tests PRIVATE /W4 /permissive- /EHsc)
    add_test(NAME stage2_winmm_transport_unit COMMAND stage2_winmm_transport_tests)
    set_tests_properties(stage2_winmm_transport_unit PROPERTIES LABELS "stage2;unit;ci;winmm")

    add_executable(stage3_winmm_transport_tests unit/Stage3WinmmTransportTests.cpp)
    target_include_directories(stage3_winmm_transport_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_link_libraries(stage3_winmm_transport_tests PRIVATE taureon_winmm_transport)
    target_compile_definitions(stage3_winmm_transport_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(stage3_winmm_transport_tests PRIVATE /W4 /permissive- /EHsc)
    add_test(NAME stage3_winmm_transport_unit COMMAND stage3_winmm_transport_tests)
    set_tests_properties(stage3_winmm_transport_unit PROPERTIES LABELS "stage3;unit;ci;winmm")
endif()

option(TAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS
    "Register local-only WMS/WinMM integration regressions" OFF)
if(TAUREON_ENABLE_LOCAL_MIDI_INTEGRATION_TESTS AND WIN32)
    add_subdirectory(integration)
endif()
