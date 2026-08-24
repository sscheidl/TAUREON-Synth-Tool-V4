add_library(taureon_winmm_transport STATIC
    WinmmMidiHeader.cpp
    WinmmNativeApi.cpp
    WinmmTransport.cpp)

target_include_directories(taureon_winmm_transport PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(taureon_winmm_transport PUBLIC taureon_midi_core PRIVATE winmm.lib)
target_compile_definitions(taureon_winmm_transport PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(taureon_winmm_transport PRIVATE /W4 /permissive- /EHsc)
