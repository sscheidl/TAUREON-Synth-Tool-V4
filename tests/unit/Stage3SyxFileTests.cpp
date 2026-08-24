#include "TestSupport.hpp"

#include "core/sysex/SyxFile.hpp"

#include <filesystem>
#include <fstream>
#include <string>
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

bool contains(const std::string& value, const std::string& fragment) {
    return value.find(fragment) != std::string::npos;
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

void syx_file_error_context_tests() {
    const auto base = std::filesystem::current_path();
    const auto missing = base / "stage3-missing-input.syx";
    const auto directory = base / "stage3-directory-input.syx";
    const auto missing_parent = base / "stage3-missing-parent";
    const auto inaccessible_temp_target = missing_parent / "write.syx";
    const auto replace_directory = base / "stage3-replace-target.syx";
    const auto replace_temp = std::filesystem::path(replace_directory.string() + ".taureon.tmp");
    Cleanup cleanup{{missing, replace_temp, directory, replace_directory}};

    std::error_code error;
    std::filesystem::remove(missing, error);
    std::filesystem::remove_all(missing_parent, error);
    std::filesystem::remove_all(directory, error);
    std::filesystem::remove_all(replace_directory, error);

    const auto missing_result = sysex::load_syx_file(missing);
    TAUREON_REQUIRE(!missing_result);
    TAUREON_REQUIRE(contains(missing_result.error().message, missing.filename().string()));
    TAUREON_REQUIRE(missing_result.error().native_code.has_value());
    TAUREON_REQUIRE(!missing_result.error().native_api.empty());

    TAUREON_REQUIRE(std::filesystem::create_directory(directory));
    const auto directory_result = sysex::load_syx_file(directory);
    TAUREON_REQUIRE(!directory_result);
    TAUREON_REQUIRE(contains(directory_result.error().message, directory.filename().string()));
    TAUREON_REQUIRE(directory_result.error().native_code.has_value());

    const std::vector<sysex::SysExFrame> frames{frame({0xf0, 0x7d, 0xf7})};
    const auto temp_open_result = sysex::save_syx_frames(inaccessible_temp_target, frames, true);
    TAUREON_REQUIRE(!temp_open_result);
    TAUREON_REQUIRE(contains(temp_open_result.error().message, "write.syx.taureon.tmp"));
    TAUREON_REQUIRE(temp_open_result.error().native_code.has_value());
    TAUREON_REQUIRE(contains(temp_open_result.error().native_api, "temporary"));

    TAUREON_REQUIRE(std::filesystem::create_directory(replace_directory));
    const auto replace_result = sysex::save_syx_frames(replace_directory, frames, true);
    TAUREON_REQUIRE(!replace_result);
    TAUREON_REQUIRE(contains(replace_result.error().message,
                             replace_directory.filename().string()));
    TAUREON_REQUIRE(contains(replace_result.error().message, replace_temp.filename().string()));
    TAUREON_REQUIRE(replace_result.error().native_code.has_value());
#ifdef _WIN32
    TAUREON_REQUIRE(replace_result.error().native_api == "MoveFileExW");
#else
    TAUREON_REQUIRE(contains(replace_result.error().native_api, "rename"));
#endif
}

} // namespace

int main() {
    return test::run([] {
        syx_file_tests();
        syx_file_error_context_tests();
    });
}
