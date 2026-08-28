#include "TestSupport.hpp"

#include "app/SysExManager.hpp"

#include <filesystem>
#include <fstream>
#include <memory>

using namespace taureon;

namespace {

std::filesystem::path output_path(const char* name) {
    return std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} / name;
}

void remove_file(const std::filesystem::path& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

} // namespace

int main() {
    return test::run([] {
        const auto fixture = std::filesystem::path{TAUREON_SOURCE_DIR} / "tests" / "fixtures" /
                             "novation_summit_crazy_sine.syx";
        auto registry = std::make_shared<profiles::ProfileRegistry>();
        std::vector<profiles::ProfileLoadIssue> issues;
        TAUREON_REQUIRE(registry->load_directory(
            std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles", issues));
        TAUREON_REQUIRE(issues.empty());

        app::SysExManager manager(registry);
        const auto first = manager.add_file(fixture);
        TAUREON_REQUIRE(first);
        const auto duplicate = manager.add_file(fixture);
        TAUREON_REQUIRE(duplicate);
        const auto snapshot = manager.snapshot();
        TAUREON_REQUIRE(snapshot.items.size() == 2);
        TAUREON_REQUIRE(snapshot.items.front().manufacturer == std::optional<std::string>{"Novation"});
        TAUREON_REQUIRE(snapshot.items.front().model == std::optional<std::string>{"Summit"});
        TAUREON_REQUIRE(snapshot.items.front().is_valid_for_transfer());
        TAUREON_REQUIRE(snapshot.items.front().file_hash.starts_with("fnv1a64:"));
        TAUREON_REQUIRE(snapshot.items.at(1).exact_file_duplicate_of == first.value());
        TAUREON_REQUIRE(!snapshot.items.front().frames.front().exact_frame_duplicates.empty());
        TAUREON_REQUIRE(!snapshot.items.front().frames.front().exact_payload_duplicates.empty());

        const auto export_destination = output_path("stage5-manager-export.syx");
        const auto merge_destination = output_path("stage5-manager-merge.syx");
        remove_file(export_destination);
        remove_file(merge_destination);
        TAUREON_REQUIRE(manager.export_frames(first.value(), {0}, export_destination));
        auto exported = sysex::load_syx_file(export_destination);
        TAUREON_REQUIRE(exported);
        TAUREON_REQUIRE(exported.value().raw_bytes == snapshot.items.front().raw_bytes);
        TAUREON_REQUIRE(manager.merge_frames({{first.value(), 0}, {duplicate.value(), 0}},
                                             merge_destination));
        auto merged = sysex::load_syx_file(merge_destination);
        TAUREON_REQUIRE(merged);
        TAUREON_REQUIRE(merged.value().frames.size() == 2);
        TAUREON_REQUIRE(merged.value().raw_bytes.size() == snapshot.items.front().raw_bytes.size() * 2);
        TAUREON_REQUIRE(std::filesystem::exists(fixture));
        TAUREON_REQUIRE(manager.remove_item(duplicate.value()));
        TAUREON_REQUIRE(std::filesystem::exists(fixture));

        const auto incomplete = output_path("stage5-manager-incomplete.syx");
        remove_file(incomplete);
        {
            std::ofstream stream(incomplete, std::ios::binary | std::ios::trunc);
            const char raw[] = {static_cast<char>(0xF0), static_cast<char>(0x7D), static_cast<char>(0x22)};
            stream.write(raw, sizeof(raw));
        }
        const auto invalid = manager.add_file(incomplete);
        TAUREON_REQUIRE(invalid);
        TAUREON_REQUIRE(!manager.transferable_source(invalid.value()));
        TAUREON_REQUIRE(!manager.export_frames(invalid.value(), {0}, output_path("never-write.syx")));
        remove_file(export_destination);
        remove_file(merge_destination);
        remove_file(incomplete);
        remove_file(output_path("never-write.syx"));
    });
}
