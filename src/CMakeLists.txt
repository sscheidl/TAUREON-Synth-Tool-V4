include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)
set(TAUREON_MIDI_ENGINE_VERSION 0.3.0)
string(REPLACE "." ";" engine_version_parts "${TAUREON_MIDI_ENGINE_VERSION}")
list(GET engine_version_parts 0 TAUREON_MIDI_ENGINE_VERSION_MAJOR)
list(GET engine_version_parts 1 TAUREON_MIDI_ENGINE_VERSION_MINOR)
list(GET engine_version_parts 2 TAUREON_MIDI_ENGINE_VERSION_PATCH)
configure_file(core/midi/EngineVersion.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/core/midi/EngineVersion.hpp" @ONLY)

add_library(taureon_midi_engine STATIC
    core/midi/MidiMessage.cpp
    core/midi/MidiTypes.cpp
    core/midi/RoutePersistence.cpp
    core/midi/RouteResolver.cpp
    core/midi/TransportState.cpp
    core/sysex/SysEx7.cpp
    core/sysex/SysExCaptureSession.cpp
    core/sysex/SysExStreamParser.cpp
    core/sysex/SyxFile.cpp
    core/transfer/TransferEngine.cpp)
add_library(TaureonMidiEngine::MidiEngine ALIAS taureon_midi_engine)
set_target_properties(taureon_midi_engine PROPERTIES EXPORT_NAME MidiEngine DEBUG_POSTFIX d)
target_include_directories(taureon_midi_engine PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_features(taureon_midi_engine PUBLIC cxx_std_20)
target_link_libraries(taureon_midi_engine PUBLIC Threads::Threads)

if(MSVC)
    target_compile_options(taureon_midi_engine PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(taureon_midi_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS taureon_midi_engine EXPORT TaureonMidiEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
# The source tree keeps short internal paths; the installed API is always
# taureon-prefixed, including includes made by one public header from another.
file(GLOB_RECURSE engine_public_headers CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/core/*.hpp")
list(APPEND engine_public_headers "${PROJECT_SOURCE_DIR}/src/transports/IMidiTransport.hpp")
set(engine_public_root "${CMAKE_CURRENT_BINARY_DIR}/generated-public/taureon")
foreach(header IN LISTS engine_public_headers)
    file(RELATIVE_PATH relative_header "${PROJECT_SOURCE_DIR}/src" "${header}")
    get_filename_component(relative_directory "${relative_header}" DIRECTORY)
    file(MAKE_DIRECTORY "${engine_public_root}/${relative_directory}")
    file(READ "${header}" public_header_contents)
    string(REPLACE "#include \"core/" "#include \"taureon/core/"
        public_header_contents "${public_header_contents}")
    string(REPLACE "#include \"transports/" "#include \"taureon/transports/"
        public_header_contents "${public_header_contents}")
    file(WRITE "${engine_public_root}/${relative_header}" "${public_header_contents}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${header}")
endforeach()
install(DIRECTORY "${engine_public_root}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/taureon" FILES_MATCHING PATTERN "*.hpp")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/generated/core/midi/EngineVersion.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/taureon/core/midi")
set(engine_package_dir "${CMAKE_INSTALL_LIBDIR}/cmake/TaureonMidiEngine")
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/TaureonMidiEngineConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/TaureonMidiEngineConfig.cmake"
    INSTALL_DESTINATION "${engine_package_dir}")
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/TaureonMidiEngineConfigVersion.cmake"
    VERSION "${TAUREON_MIDI_ENGINE_VERSION}" COMPATIBILITY ExactVersion)
install(EXPORT TaureonMidiEngineTargets NAMESPACE TaureonMidiEngine::
    DESTINATION "${engine_package_dir}")
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/TaureonMidiEngineConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/TaureonMidiEngineConfigVersion.cmake"
    DESTINATION "${engine_package_dir}")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/TaureonMidiEngine")

if(BUILD_TESTING AND PROJECT_IS_TOP_LEVEL)
    add_library(taureon_midi_engine_testing STATIC
        transports/fake/FakeMidiTransport.cpp)
    target_include_directories(taureon_midi_engine_testing PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(taureon_midi_engine_testing PUBLIC taureon_midi_engine)
    if(MSVC)
        target_compile_options(taureon_midi_engine_testing PRIVATE /W4 /permissive- /EHsc)
    else()
        target_compile_options(taureon_midi_engine_testing PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(NOT TAUREON_BUILD_PRODUCT_APP)
    return()
endif()

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
    profiles/ProfileLoader.cpp
    profiles/ProfileRegistry.cpp)

target_include_directories(taureon_midi_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(taureon_midi_core PUBLIC cxx_std_20)
target_link_libraries(taureon_midi_core PUBLIC taureon_midi_engine)

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
