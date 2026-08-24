add_library(taureon_midi_core STATIC
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
    app/taureon_app_main.cpp
    gui/MainWindow.cpp
    gui/MainWindow.hpp)
target_link_libraries(taureon_app PRIVATE taureon_midi_core Qt6::Widgets)
target_compile_features(taureon_app PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(taureon_app PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(taureon_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_subdirectory(transports/winmm)
    add_subdirectory(transports/wms)
endif()
