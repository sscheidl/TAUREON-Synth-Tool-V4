#include <taureon/transports/wms/WmsTransport.hpp>

#include <windows.h>

#include <filesystem>
#include <iostream>
#include <string_view>

int wmain(int argc, wchar_t* argv[]) {
    taureon::midi::wms::WmsTransport transport;
    const auto capabilities = transport.capabilities();
    const bool valid = transport.backend() == taureon::midi::MidiBackend::windows_midi_services &&
                   capabilities.supports_input && capabilities.supports_output &&
                   capabilities.supports_ump;
    if (!valid) return 1;
    if (argc == 1) return 0;
    if (argc == 2 && std::wstring_view{argv[1]} == L"--expect-unavailable") {
        const auto endpoints = transport.enumerate();
        if (endpoints || endpoints.error().code != taureon::midi::MidiErrorCode::backend_unavailable ||
            GetModuleHandleW(L"Windows.Devices.Midi2.dll") != nullptr ||
            transport.diagnostics().wms_api_system_provided.has_value()) return 1;
        std::cout << "WMS unavailable; API not loaded; " << endpoints.error().message << '\n';
        if (endpoints.error().native_code) std::cout << "Native error: " << *endpoints.error().native_code << '\n';
        return 0;
    }
    if (argc != 3 || std::wstring_view{argv[1]} != L"--probe-api") {
        std::cerr << "Expected --probe-api <expected API DLL path> or --expect-unavailable\n";
        return 2;
    }

    // Explicit opt-in read-only check, never an ordinary CTest/hardware test.
    // Enumeration uses the real transport but opens no endpoint and sends no data.
    const auto endpoints = transport.enumerate();
    if (!endpoints) {
        std::cerr << "WMS enumeration failed (error code "
                  << static_cast<int>(endpoints.error().code) << ")\n";
        return 1;
    }
    const auto module = GetModuleHandleW(L"Windows.Devices.Midi2.dll");
    std::wstring loaded_path(32768, L'\0');
    const auto length = module ? GetModuleFileNameW(
        module, loaded_path.data(), static_cast<DWORD>(loaded_path.size())) : 0;
    if (length == 0 || length >= loaded_path.size()) {
        std::cerr << "Cannot identify the loaded WMS API\n";
        return 1;
    }
    loaded_path.resize(length);
    std::error_code error;
    if (!std::filesystem::equivalent(loaded_path, std::filesystem::path{argv[2]}, error) || error) {
        std::cerr << "Loaded WMS API does not match the expected file\n";
        return 1;
    }
    std::size_t inputs = 0;
    std::size_t outputs = 0;
    for (const auto& endpoint : endpoints.value()) {
        if (endpoint.identity.direction == taureon::midi::MidiDirection::input) ++inputs;
        else ++outputs;
    }
    std::cout << "Expected WMS API loaded; " << inputs << " RX / " << outputs
              << " TX routes; enumeration only, no endpoint opened\n";
    const auto system_api = transport.diagnostics().wms_api_system_provided;
    if (!system_api.has_value()) return 1;
    std::cout << "IsSystemProvided: " << (*system_api ? "true" : "false") << '\n';
    return 0;
}
