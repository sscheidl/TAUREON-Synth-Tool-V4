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

    file(SHA256 "${wms_winmd}" wms_winmd_sha256)
    if(NOT wms_winmd_sha256 STREQUAL
            "49ab3bbe0954bf4f4d0d1ab933564974495762a11b9b02602f493c6c755c47bc")
        message(FATAL_ERROR
            "WMS WINMD does not match pinned Preview 11 (0.99.92-preview.11). "
            "Run tools/AcquireWmsDependencies.ps1 and select its Preview-11 "
            "directory with TAUREON_WMS_DEPENDENCY_ROOT. "
            "Actual SHA-256: ${wms_winmd_sha256}")
    endif()

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

# Public redistribution is separately gated by the Go-live agreement. Turning
# this off also removes stale preview files when rebuilding the same output tree.
option(TAUREON_DEPLOY_WMS_PREVIEW_API
    "Deploy app-local Preview 11 API for development (distribution requires Go-live terms)" ON)

function(taureon_remove_wms_preview_runtime target)
    add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E rm -f
                "$<TARGET_FILE_DIR:${target}>/Windows.Devices.Midi2.dll"
                "$<TARGET_FILE_DIR:${target}>/Windows.Devices.Midi2.pri"
                "$<TARGET_FILE_DIR:${target}>/Windows.Devices.Midi2.winmd"
                "$<TARGET_FILE_DIR:${target}>/Windows.Devices.Midi2.pdb"
        VERBATIM)
endfunction()

function(taureon_deploy_wms_runtime target)
    if(NOT TAUREON_DEPLOY_WMS_PREVIEW_API)
        taureon_remove_wms_preview_runtime(${target})
        return()
    endif()
    set(runtime_root "${TAUREON_WMS_DEPENDENCY_ROOT}/package/runtimes/win-x64/native")
    foreach(runtime_file IN ITEMS Windows.Devices.Midi2.dll Windows.Devices.Midi2.pri)
        if(NOT EXISTS "${runtime_root}/${runtime_file}")
            message(FATAL_ERROR "Missing Windows MIDI Services API file: ${runtime_root}/${runtime_file}")
        endif()
        if(runtime_file STREQUAL "Windows.Devices.Midi2.dll")
            set(expected_hash "db3544cac751f174da338fb43e754974a067432ce30ece9845cfcf3b3daa4cdc")
        else()
            set(expected_hash "d04a030d84c77c6be8c0f2018a6d70fcd142ab5c940644b74223523f975da9fa")
        endif()
        file(SHA256 "${runtime_root}/${runtime_file}" runtime_hash)
        if(NOT runtime_hash STREQUAL expected_hash)
            message(FATAL_ERROR "WMS Preview 11 runtime hash mismatch: ${runtime_file}")
        endif()
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${runtime_root}/${runtime_file}" "$<TARGET_FILE_DIR:${target}>/${runtime_file}"
            VERBATIM)
    endforeach()
endfunction()
