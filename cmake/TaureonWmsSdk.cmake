function(taureon_configure_wms_projection target)
    set(wms_package_root "${TAUREON_WMS_DEPENDENCY_ROOT}/package")
    set(cppwinrt_package_root "${TAUREON_WMS_DEPENDENCY_ROOT}/cppwinrt-package")
    set(cppwinrt_tool "${cppwinrt_package_root}/bin/cppwinrt.exe")
    set(wms_winmd "${wms_package_root}/ref/native/Microsoft.Windows.Devices.Midi2.winmd")
    set(generated_root "${CMAKE_CURRENT_BINARY_DIR}/generated")
    set(projection_stamp "${generated_root}/projection.stamp")

    foreach(required_file IN ITEMS "${cppwinrt_tool}" "${wms_winmd}")
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR
                "Missing pinned WMS dependency: ${required_file}. "
                "Run tools/AcquireStage1Dependencies.ps1 or set TAUREON_WMS_DEPENDENCY_ROOT.")
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
    target_link_libraries(${target} PRIVATE
        OneCoreUap.lib
        ole32.lib
        runtimeobject.lib
        "${cppwinrt_package_root}/build/native/lib/x64/cppwinrt_fast_forwarder.lib")
endfunction()
