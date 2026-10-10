function(taureon_configure_wms_projection target)
    set(wms_package_root "${TAUREON_WMS_DEPENDENCY_ROOT}/package")
    set(cppwinrt_package_root "${TAUREON_WMS_DEPENDENCY_ROOT}/cppwinrt-package")
    set(cppwinrt_tool "${cppwinrt_package_root}/bin/cppwinrt.exe")
    set(wms_winmd "${wms_package_root}/ref/native/Windows.Devices.Midi2.winmd")
    set(cppwinrt_fast_forwarder
        "${cppwinrt_package_root}/build/native/lib/x64/cppwinrt_fast_forwarder.lib")
    set(generated_root "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(projection_stamp "${generated_root}/projection.stamp")

    foreach(required_file IN ITEMS
            "${cppwinrt_tool}" "${wms_winmd}" "${cppwinrt_fast_forwarder}")
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR
                "Missing pinned WMS dependency: ${required_file}. "
                "Run tools/AcquireWmsDependencies.ps1 or set TAUREON_WMS_DEPENDENCY_ROOT.")
        endif()
    endforeach()

    add_custom_command(
        OUTPUT "${projection_stamp}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${generated_root}"
        COMMAND "${cppwinrt_tool}" -input "${TAUREON_WINDOWS_SDK_CONTRACT_VERSION}"
                -output "${generated_root}" -overwrite
        COMMAND "${cppwinrt_tool}" -input "${wms_winmd}"
                -reference "${TAUREON_WINDOWS_SDK_CONTRACT_VERSION}"
                -output "${generated_root}" -overwrite
        COMMAND "${CMAKE_COMMAND}" -E touch "${projection_stamp}"
        DEPENDS "${cppwinrt_tool}" "${wms_winmd}"
        COMMENT "Generating pinned WMS C++/WinRT projection"
        VERBATIM)
    add_custom_target(${target}_projection DEPENDS "${projection_stamp}")
    add_dependencies(${target} ${target}_projection)

    target_include_directories(${target} PRIVATE
        "${generated_root}"
        "${wms_package_root}/build/native/include")
    if(NOT TARGET TaureonMidiEngine::CppWinRTFastForwarder)
        add_library(TaureonMidiEngine::CppWinRTFastForwarder STATIC IMPORTED GLOBAL)
        set_target_properties(TaureonMidiEngine::CppWinRTFastForwarder PROPERTIES
            IMPORTED_LOCATION "${cppwinrt_fast_forwarder}")
    endif()
    target_link_libraries(${target} PRIVATE
        OneCoreUap.lib
        ole32.lib
        runtimeobject.lib
        TaureonMidiEngine::CppWinRTFastForwarder)
    install(FILES "${cppwinrt_fast_forwarder}"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}")
endfunction()

# Internal test builds may deploy the Preview 9 API beside the executable,
# following Microsoft's native NuGet targets. This does not install/register
# a system runtime. Public preview distribution has additional upstream terms.
function(taureon_deploy_wms_runtime target)
    set(runtime_root "${TAUREON_WMS_DEPENDENCY_ROOT}/package/runtimes/win-x64/native")
    foreach(runtime_file IN ITEMS Windows.Devices.Midi2.dll Windows.Devices.Midi2.pri)
        if(NOT EXISTS "${runtime_root}/${runtime_file}")
            message(FATAL_ERROR "Missing Preview 9 runtime file: ${runtime_root}/${runtime_file}")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_root}/${runtime_file}" "$<TARGET_FILE_DIR:${target}>/${runtime_file}"
            VERBATIM)
    endforeach()
endfunction()
