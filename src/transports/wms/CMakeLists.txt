set(TAUREON_WMS_DEPENDENCY_ROOT "${PROJECT_SOURCE_DIR}/build/dependencies/wms-preview10" CACHE PATH
    "Project-local pinned WMS SDK and C++/WinRT packages")
set(TAUREON_WINDOWS_SDK_CONTRACT_VERSION "10.0.26100.0" CACHE STRING
    "Windows SDK contract version used for C++/WinRT projection")

set(wms_dependencies_available OFF)
if(EXISTS "${TAUREON_WMS_DEPENDENCY_ROOT}/package/ref/native/Windows.Devices.Midi2.winmd"
   AND EXISTS "${TAUREON_WMS_DEPENDENCY_ROOT}/cppwinrt-package/bin/cppwinrt.exe")
    set(wms_dependencies_available ON)
endif()
option(TAUREON_ENABLE_WMS_TRANSPORT
    "Build the production WMS transport when pinned dependencies are available"
    ${wms_dependencies_available})

if(TAUREON_ENABLE_WMS_TRANSPORT)
    add_library(taureon_wms_transport STATIC WmsTransport.cpp)
    add_library(TaureonMidiEngine::WmsTransport ALIAS taureon_wms_transport)
    set_target_properties(taureon_wms_transport PROPERTIES
        EXPORT_NAME WmsTransport
        DEBUG_POSTFIX d)
    target_include_directories(taureon_wms_transport
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(taureon_wms_transport PUBLIC taureon_midi_engine)
    target_compile_features(taureon_wms_transport PUBLIC cxx_std_20)
    target_compile_definitions(taureon_wms_transport PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(taureon_wms_transport PRIVATE /W4 /permissive- /bigobj /EHsc)
    include("${PROJECT_SOURCE_DIR}/cmake/TaureonWmsSdk.cmake")
    taureon_configure_wms_projection(taureon_wms_transport)
    install(TARGETS taureon_wms_transport EXPORT TaureonMidiEngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
    install(FILES "${PROJECT_SOURCE_DIR}/include/taureon/transports/wms/WmsTransport.hpp"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/taureon/transports/wms")
else()
    message(STATUS "Production WMS transport disabled: pinned project-local SDK unavailable or option OFF")
endif()
