#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.Windows.Devices.Midi2.h>

#include "winmidi/init/Microsoft.Windows.Devices.Midi2.Initialization.hpp"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace midi = winrt::Microsoft::Windows::Devices::Midi2;
namespace init = Microsoft::Windows::Devices::Midi2::Initialization;

namespace {

std::string json_escape(const std::string& value) {
    std::string result;
    for (const char ch : value) {
        if (ch == '\\') result += "\\\\";
        else if (ch == '"') result += "\\\"";
        else result += ch;
    }
    return result;
}

winrt::hstring to_hstring(const std::string& value) {
    return winrt::to_hstring(value);
}

bool contains_exact(const winrt::Windows::Foundation::Collections::IVectorView<winrt::hstring>& values,
                    const winrt::hstring& expected) {
    for (const auto& value : values) {
        if (value == expected) return true;
    }
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: taureon_wms_winmm_correlation "
                     "<input-index> <input-name> <output-index> <output-name>"
                  << std::endl;
        return EXIT_FAILURE;
    }

    const auto input_index = static_cast<std::uint32_t>(std::stoul(argv[1]));
    const auto input_name = to_hstring(argv[2]);
    const auto output_index = static_cast<std::uint32_t>(std::stoul(argv[3]));
    const auto output_name = to_hstring(argv[4]);

    bool pass = false;
    std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"before_apartment\"}" << std::endl;
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"after_apartment\"}" << std::endl;
    auto initializer = std::make_shared<init::MidiDesktopAppSdkInitializer>();
    try {
        const bool initialized = initializer->InitializeSdkRuntime();
        std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"sdk_initialized\","
                  << "\"ok\":" << (initialized ? "true" : "false") << "}" << std::endl;
        const bool version_ok = initialized && initializer->CheckForMinimumRequiredSdkVersion(1, 0, 17);
        std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"version_checked\","
                  << "\"ok\":" << (version_ok ? "true" : "false") << "}" << std::endl;
        const bool service_ok = version_ok && initializer->EnsureServiceAvailable();
        std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"service_available\","
                  << "\"ok\":" << (service_ok ? "true" : "false") << "}" << std::endl;
        if (!service_ok) {
            throw std::runtime_error("WMS runtime initialization failed");
        }

        {
            std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"before_input_number\"}"
                      << std::endl;
            const auto input_by_number = midi::MidiEndpointDeviceInformation::
                FindEndpointDeviceIdForAssociatedMidi1PortNumber(
                    input_index, midi::Midi1PortFlow::MidiMessageSource);
            std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"input_number_resolved\","
                      << "\"endpoint_id\":\""
                      << json_escape(winrt::to_string(input_by_number)) << "\"}" << std::endl;
            std::cout << "{\"event\":\"correlation_phase\",\"phase\":\"before_output_number\"}"
                      << std::endl;
            const auto output_by_number = midi::MidiEndpointDeviceInformation::
                FindEndpointDeviceIdForAssociatedMidi1PortNumber(
                    output_index, midi::Midi1PortFlow::MidiMessageDestination);
            const auto input_endpoint = midi::MidiEndpointDeviceInformation::
                CreateFromEndpointDeviceId(input_by_number);
            const auto output_endpoint = midi::MidiEndpointDeviceInformation::
                CreateFromEndpointDeviceId(output_by_number);

            const bool input_consistent = !input_by_number.empty() && input_endpoint &&
                                          input_endpoint.Name() == input_name;
            const bool output_consistent = !output_by_number.empty() && output_endpoint &&
                                           output_endpoint.Name() == output_name;
            pass = input_consistent && output_consistent;
            std::cout << "{\"event\":\"winmm_wms_correlation\",\"status\":\""
                      << (pass ? "consistent" : "inconsistent")
                      << "\",\"input_number_id\":\""
                      << json_escape(winrt::to_string(input_by_number))
                      << "\",\"input_endpoint_name\":\""
                      << json_escape(winrt::to_string(input_endpoint.Name()))
                      << "\",\"output_number_id\":\""
                      << json_escape(winrt::to_string(output_by_number))
                      << "\",\"output_endpoint_name\":\""
                      << json_escape(winrt::to_string(output_endpoint.Name()))
                      << "\",\"name_helper_status\":\"reproducible_fail_fast_in_separate_probe\""
                      << ",\"architecture_conclusion\":false}" << std::endl;
        }
    } catch (const winrt::hresult_error& error) {
        std::cout << "{\"event\":\"winmm_wms_correlation\",\"status\":\"api_error\","
                  << "\"hresult\":" << static_cast<std::int32_t>(error.code())
                  << ",\"message\":\""
                  << json_escape(winrt::to_string(error.message())) << "\"}" << std::endl;
    } catch (const std::exception& error) {
        std::cout << "{\"event\":\"winmm_wms_correlation\",\"status\":\"error\","
                  << "\"message\":\"" << json_escape(error.what()) << "\"}" << std::endl;
    }

    initializer->ShutdownSdkRuntime();
    initializer.reset();
    winrt::uninit_apartment();
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}
