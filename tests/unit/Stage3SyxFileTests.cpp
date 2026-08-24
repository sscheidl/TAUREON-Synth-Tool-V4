#include "TestSupport.hpp"

#include "core/sysex/SyxFile.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

using namespace taureon;

namespace {

struct Cleanup {
    std::vector<std::filesystem::path> paths;
    ~Cleanup() {
        std::error_code error;
        for (const auto& path : paths) std::filesystem::remove(path, error);
    }
};

sysex::SysExFrame frame(std::vector<std::uint8_t> bytes) {
    return {sysex::SysExFrameStatus::complete, std::move(bytes), {}, std::nullopt, false};
}

void syx_file_tests() {
    const auto base = std::filesystem::current_path();
    const auto valid_path = base / "stage3-valid-roundtrip.syx";
    const auto copy_path = base / "stage3-valid-copy.syx";
    const auto incomplete_path = base / "stage3-incomplete.syx";
    const auto raw_path = base / "stage3-explicit-raw.syx";
    Cleanup cleanup{{valid_path, copy_path, incomplete_path, raw_path,
                     valid_path.string() + ".taureon.tmp",
                     copy_path.string() + ".taureon.tmp",
                     incomplete_path.string() + ".taureon.tmp",
                     raw_path.string() + ".taureon.tmp"}};
    for (const auto& path : cleanup.paths) {
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    const std::vector<sysex::SysExFrame> frames{
        frame({0xf0, 0x7d, 1, 0xf7}), frame({0xf0, 2, 3, 4, 0xf7})};
    TAUREON_REQUIRE(sysex::save_syx_frames(valid_path, frames));
    TAUREON_REQUIRE(!sysex::save_syx_frames(valid_path, frames));
    const auto loaded = sysex::load_syx_file(valid_path);
    TAUREON_REQUIRE(loaded);
    TAUREON_REQUIRE(loaded.value().all_complete());
    TAUREON_REQUIRE(loaded.value().complete_frame_count() == 2);
    TAUREON_REQUIRE(loaded.value().frames == frames);
    TAUREON_REQUIRE(sysex::save_syx_frames(copy_path, loaded.value().frames));
    const auto copied = sysex::load_syx_file(copy_path);
    TAUREON_REQUIRE(copied);
    TAUREON_REQUIRE(copied.value().raw_bytes == loaded.value().raw_bytes);

    {
        std::ofstream stream(incomplete_path, std::ios::binary);
        const std::vector<std::uint8_t> bytes{0xf0, 1, 2, 3};
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
    }
    const auto incomplete = sysex::load_syx_file(incomplete_path);
    TAUREON_REQUIRE(incomplete);
    TAUREON_REQUIRE(!incomplete.value().all_complete());
    TAUREON_REQUIRE(incomplete.value().frames.size() == 1);
    TAUREON_REQUIRE(incomplete.value().frames.front().status ==
                    sysex::SysExFrameStatus::incomplete);
    TAUREON_REQUIRE(!sysex::save_syx_frames(raw_path, incomplete.value().frames));
    TAUREON_REQUIRE(!sysex::save_syx_raw(raw_path, incomplete.value(), false));
    TAUREON_REQUIRE(sysex::save_syx_raw(raw_path, incomplete.value(), true));
    TAUREON_REQUIRE(sysex::load_syx_file(raw_path).value().raw_bytes ==
                    incomplete.value().raw_bytes);
}

} // namespace

int main() { return test::run([] { syx_file_tests(); }); }
