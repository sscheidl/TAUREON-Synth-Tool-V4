add_executable(taureon_winmm_spike winmm_spike_main.cpp)
target_include_directories(taureon_winmm_spike PRIVATE
    "${PROJECT_SOURCE_DIR}/spikes")
target_link_libraries(taureon_winmm_spike PRIVATE winmm.lib)
target_compile_definitions(taureon_winmm_spike PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(taureon_winmm_spike PRIVATE /W4 /permissive- /bigobj /EHsc)

if(BUILD_TESTING)
    add_test(NAME stage1_winmm_identity_logic COMMAND taureon_winmm_spike --identity-only)
    set_tests_properties(stage1_winmm_identity_logic PROPERTIES LABELS "stage1;winmm")
endif()
