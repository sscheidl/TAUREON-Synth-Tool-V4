cmake_minimum_required(VERSION 3.25)
project(TaureonMidiEngineConsumer LANGUAGES CXX)

option(TAUREON_CONSUMER_WITH_WINMM "Build the installed WinMM consumer" ON)
option(TAUREON_CONSUMER_WITH_WMS "Build the installed WMS consumer" OFF)
set(taureon_components Core)
if(TAUREON_CONSUMER_WITH_WINMM)
    list(APPEND taureon_components WinMM)
endif()
if(TAUREON_CONSUMER_WITH_WMS)
    list(APPEND taureon_components WMS)
endif()
find_package(TaureonMidiEngine 0.3.0 EXACT CONFIG REQUIRED
    COMPONENTS ${taureon_components})

add_executable(taureon_engine_consumer main.cpp)
target_link_libraries(taureon_engine_consumer PRIVATE TaureonMidiEngine::MidiEngine)

if(TAUREON_CONSUMER_WITH_WINMM)
    add_executable(taureon_winmm_consumer winmm_main.cpp)
    target_link_libraries(taureon_winmm_consumer PRIVATE TaureonMidiEngine::WinmmTransport)
endif()

if(TAUREON_CONSUMER_WITH_WMS)
    add_executable(taureon_wms_consumer wms_main.cpp)
    target_link_libraries(taureon_wms_consumer PRIVATE TaureonMidiEngine::WmsTransport)
endif()

enable_testing()
add_test(NAME taureon_engine_consumer_runs COMMAND taureon_engine_consumer)
if(TAUREON_CONSUMER_WITH_WINMM)
    add_test(NAME taureon_winmm_consumer_runs COMMAND taureon_winmm_consumer)
endif()
