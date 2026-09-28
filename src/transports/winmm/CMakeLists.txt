add_library(taureon_winmm_transport STATIC
    WinmmMidiHeader.cpp
    WinmmNativeApi.cpp
    WinmmTransport.cpp)
add_library(TaureonMidiEngine::WinmmTransport ALIAS taureon_winmm_transport)
set_target_properties(taureon_winmm_transport PROPERTIES
    EXPORT_NAME WinmmTransport
    DEBUG_POSTFIX d)

target_include_directories(taureon_winmm_transport
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(taureon_winmm_transport PUBLIC taureon_midi_engine PRIVATE winmm.lib)
target_compile_features(taureon_winmm_transport PUBLIC cxx_std_20)
target_compile_definitions(taureon_winmm_transport PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(taureon_winmm_transport PRIVATE /W4 /permissive- /EHsc)

install(TARGETS taureon_winmm_transport EXPORT TaureonMidiEngineTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES "${PROJECT_SOURCE_DIR}/include/taureon/transports/winmm/WinmmTransport.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/taureon/transports/winmm")
