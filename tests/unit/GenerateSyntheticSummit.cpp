#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

// This generates recognition-shaped test data, not a device-valid preset.
// The output is a disposable build artifact and must never be sent to hardware.
int main(int argc, char** argv) {
    if (argc != 2) return 1;
    constexpr std::array<std::uint8_t, 8> prefix{
        0xf0, 0x00, 0x20, 0x29, 0x01, 0x11, 0x01, 0x33};
    constexpr std::string_view marker = "TAUREON SYNTHETIC TEST DATA - DO NOT SEND";
    std::vector<std::uint8_t> frame(prefix.begin(), prefix.end());
    frame.insert(frame.end(), marker.begin(), marker.end());
    while (frame.size() < 526) {
        const auto index = frame.size() - prefix.size() - marker.size();
        frame.push_back(static_cast<std::uint8_t>((index * 37 + 11) % 127));
    }
    frame.push_back(0xf7);
    const std::filesystem::path output{argv[1]};
    std::filesystem::create_directories(output.parent_path());
    std::ofstream stream(output, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(frame.data()),
                 static_cast<std::streamsize>(frame.size()));
    return stream.good() ? 0 : 2;
}
