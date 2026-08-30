add_library(taureon_midi_core STATIC
    app/BoundedLog.cpp
    app/ConnectionController.cpp
    app/Diagnostics.cpp
    app/Librarian.cpp
    app/Settings.cpp
    app/MonitorEventQueue.cpp
    app/ProfileSelectionService.cpp
    app/SysExTransferSession.cpp
    app/SysExManager.cpp
    core/midi/MidiMessage.cpp
    core/midi/MidiTypes.cpp
    core/midi/RoutePersistence.cpp
    core/midi/RouteResolver.cpp
    core/midi/TransportState.cpp
    core/sysex/SysEx7.cpp
    core/sysex/SysExCaptureSession.cpp
    core/sysex/SysExStreamParser.cpp
    core/sysex/SyxFile.cpp
    core/transfer/TransferEngine.cpp
    profiles/ProfileLoader.cpp
    profiles/ProfileRegistry.cpp
    transports/fake/FakeMidiTransport.cpp)

target_include_directories(taureon_midi_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(taureon_midi_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(taureon_midi_core PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(taureon_midi_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Qt6 6.10 REQUIRED COMPONENTS Widgets)

add_executable(taureon_app
    app/ConnectionWorker.cpp
    app/ConnectionWorker.hpp
    app/NativeTransportFactory.cpp
    app/NativeTransportFactory.hpp
    app/taureon_app_main.cpp
    gui/DiagnosticsPanel.cpp
    gui/DiagnosticsPanel.hpp
    gui/LibrarianPanel.cpp
    gui/LibrarianPanel.hpp
    gui/MainWindow.cpp
    gui/MainWindow.hpp
    gui/MidiMonitorFilterModel.cpp
    gui/MidiMonitorFilterModel.hpp
    gui/MonitorEventBridge.cpp
    gui/MonitorEventBridge.hpp
    gui/MidiMonitorModel.cpp
    gui/MidiMonitorModel.hpp
    gui/ProfileMatchPanel.cpp
    gui/ProfileMatchPanel.hpp
    gui/SettingsPanel.cpp
    gui/SettingsPanel.hpp
    gui/SysExTransferPanel.cpp
    gui/SysExTransferPanel.hpp
    gui/SysExManagerPanel.cpp
    gui/SysExManagerPanel.hpp)
set_target_properties(taureon_app PROPERTIES AUTOMOC ON)
target_link_libraries(taureon_app PRIVATE taureon_midi_core Qt6::Widgets)
target_compile_features(taureon_app PRIVATE cxx_std_20)
target_compile_definitions(taureon_app PRIVATE
    TAUREON_APP_VERSION="${PROJECT_VERSION}"
    TAUREON_BUILD_REVISION="not embedded")
add_custom_command(TARGET taureon_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/resources/device_profiles"
        "$<TARGET_FILE_DIR:taureon_app>/resources/device_profiles"
    COMMENT "Deploying Stage 4 device profiles beside taureon_app")
if(MSVC)
    target_compile_options(taureon_app PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(taureon_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_subdirectory(transports/winmm)
    add_subdirectory(transports/wms)
    target_link_libraries(taureon_app PRIVATE taureon_winmm_transport)
    if(TARGET taureon_wms_transport)
        target_link_libraries(taureon_app PRIVATE taureon_wms_transport)
        target_compile_definitions(taureon_app PRIVATE TAUREON_HAS_WMS_TRANSPORT=1)
    endif()
endif()
