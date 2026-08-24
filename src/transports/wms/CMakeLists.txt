set(TAUREON_WMS_DEPENDENCY_ROOT "${PROJECT_SOURCE_DIR}/build/dependencies/wms-rc4" CACHE PATH
    "Project-local pinned WMS SDK and C++/WinRT packages")
set(TAUREON_WINDOWS_SDK_CONTRACT_VERSION "10.0.26100.0" CACHE STRING
    "Windows SDK contract version used for C++/WinRT projection")

set(wms_dependencies_available OFF)
if(EXISTS "${TAUREON_WMS_DEPENDENCY_ROOT}/package/ref/native/Microsoft.Windows.Devices.Midi2.winmd"
   AND EXISTS "${TAUREON_WMS_DEPENDENCY_ROOT}/cppwinrt-package/bin/cppwinrt.exe")
    set(wms_dependencies_available ON)
endif()
option(TAUREON_ENABLE_WMS_TRANSPORT
    "Build the production WMS transport when pinned dependencies are available"
    ${wms_dependencies_available})

if(TAUREON_ENABLE_WMS_TRANSPORT)
    add_library(taureon_wms_transport STATIC WmsTransport.cpp)
    target_include_directories(taureon_wms_transport PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(taureon_wms_transport PUBLIC taureon_midi_core)
    target_compile_definitions(taureon_wms_transport PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(taureon_wms_transport PRIVATE /W4 /permissive- /bigobj /EHsc)
    include("${PROJECT_SOURCE_DIR}/cmake/TaureonWmsSdk.cmake")
    taureon_configure_wms_projection(taureon_wms_transport)
else()
    message(STATUS "Production WMS transport disabled: pinned project-local SDK unavailable or option OFF")
endif()
