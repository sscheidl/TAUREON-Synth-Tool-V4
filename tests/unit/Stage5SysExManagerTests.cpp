#include "TestSupport.hpp"

#include "app/SysExManager.hpp"

#include <filesystem>
#include <string>
#include <fstream>
#include <iterator>
#include <memory>
#include <vector>

using namespace taureon;

namespace {

std::filesystem::path output_path(const char* name) {
    return std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} / name;
}

void remove_file(const std::filesystem::path& path) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(std::filesystem::path{path.string() + ".taureon.tmp"}, ignored);
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

std::vector<std::uint8_t> read_bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> large_frame(const std::size_t bytes) {
    std::vector<std::uint8_t> result(bytes, 0x01);
    result.front() = 0xF0;
    result.back() = 0xF7;
    return result;
}

} // namespace

int main() {
    return test::run([] {
        const auto fixture = std::filesystem::path{TAUREON_SOURCE_DIR} / "tests" / "fixtures" /
                             "novation_summit_crazy_sine.syx";
        const auto fixture_bytes = read_bytes(fixture);
        TAUREON_REQUIRE(!fixture_bytes.empty());

        auto registry = std::make_shared<profiles::ProfileRegistry>();
        std::vector<profiles::ProfileLoadIssue> issues;
        TAUREON_REQUIRE(registry->load_directory(
            std::filesystem::path{TAUREON_SOURCE_DIR} / "resources" / "device_profiles", issues));
        TAUREON_REQUIRE(issues.empty());

        const auto export_destination = output_path("stage5-manager-export.syx");
        const auto merge_destination = output_path("stage5-manager-merge.syx");
        const auto replace_destination = output_path("stage5-manager-replace.syx");
        const auto incomplete_path = output_path("stage5-manager-incomplete.syx");
        const auto malformed_path = output_path("stage5-manager-malformed.syx");
        const auto first_distinct_path = output_path("stage5-manager-distinct-one.syx");
        const auto second_distinct_path = output_path("stage5-manager-distinct-two.syx");
        const auto no_output_parent = output_path("stage5-manager-no-output-parent");
        const auto failed_export = no_output_parent / "failed-export.syx";
        const auto oversized_path = output_path("stage5-manager-oversized.syx");
        std::error_code ignored;
        std::filesystem::remove_all(no_output_parent, ignored);
        for (const auto& path : {export_destination, merge_destination, replace_destination,
                                 incomplete_path, malformed_path, first_distinct_path,
                                 second_distinct_path, failed_export, oversized_path}) {
            remove_file(path);
        }
        {
            std::ofstream oversized(oversized_path, std::ios::binary | std::ios::trunc);
        }
        std::filesystem::resize_file(oversized_path, app::SysExManager::kMaxDocumentRawBytes + 1);

        app::SysExManager manager(registry);
        const auto oversized = manager.add_file(oversized_path);
        TAUREON_REQUIRE(!oversized);
        TAUREON_REQUIRE(oversized.error().code == midi::MidiErrorCode::resource_limit_exceeded);
        TAUREON_REQUIRE(oversized.error().message.find("document file") != std::string::npos);
        TAUREON_REQUIRE(manager.snapshot().items.empty());

        const auto unknown_size = manager.add_file(output_path("stage5-manager-missing.syx"));
        TAUREON_REQUIRE(!unknown_size);
        TAUREON_REQUIRE(unknown_size.error().code == midi::MidiErrorCode::io_error);

        const auto aggregate_chunk = app::SysExManager::kMaxAggregateRawBytes / 3 + 1;
        const auto aggregate_document = [aggregate_chunk] {
            sysex::SyxDocument document;
            document.raw_bytes.assign(aggregate_chunk, 0x7D);
            return document;
        };
        app::SysExManager aggregate_manager;
        const auto aggregate_first = aggregate_manager.add_document(aggregate_document(), "aggregate-one.syx");
        const auto aggregate_second = aggregate_manager.add_document(aggregate_document(), "aggregate-two.syx");
        TAUREON_REQUIRE(aggregate_first && aggregate_second);
        const auto aggregate_rejected =
            aggregate_manager.add_document(aggregate_document(), "aggregate-three.syx");
        TAUREON_REQUIRE(!aggregate_rejected);
        TAUREON_REQUIRE(aggregate_rejected.error().code ==
                        midi::MidiErrorCode::resource_limit_exceeded);
        TAUREON_REQUIRE(aggregate_manager.snapshot().items.size() == 2);
        TAUREON_REQUIRE(aggregate_manager.remove_item(aggregate_first.value()));
        TAUREON_REQUIRE(aggregate_manager.add_document(aggregate_document(), "aggregate-three.syx"));

        const auto first = manager.add_file(fixture);
        TAUREON_REQUIRE(first);
        const auto duplicate = manager.add_file(fixture);
        TAUREON_REQUIRE(duplicate);
        const auto snapshot = manager.snapshot();
        TAUREON_REQUIRE(snapshot.items.size() == 2);
        TAUREON_REQUIRE(snapshot.items.front().manufacturer == std::optional<std::string>{"Novation"});
        TAUREON_REQUIRE(snapshot.items.front().model == std::optional<std::string>{"Summit"});
        TAUREON_REQUIRE(snapshot.items.front().is_valid_for_transfer());
        TAUREON_REQUIRE(manager.can_transfer(first.value()));
        const auto transfer_item = manager.transferable_item(first.value());
        TAUREON_REQUIRE(transfer_item);
        TAUREON_REQUIRE(transfer_item->document.raw_bytes == fixture_bytes);
        TAUREON_REQUIRE(snapshot.items.front().byte_count == fixture_bytes.size());
        TAUREON_REQUIRE(snapshot.items.front().file_hash.starts_with("fnv1a64:"));
        TAUREON_REQUIRE(snapshot.items.at(1).exact_file_duplicate_of == first.value());
        TAUREON_REQUIRE(!snapshot.items.front().frames.front().exact_frame_duplicates.empty());
        TAUREON_REQUIRE(!snapshot.items.front().frames.front().exact_payload_duplicates.empty());

        TAUREON_REQUIRE(manager.export_frames(first.value(), {0}, export_destination));
        TAUREON_REQUIRE(read_bytes(export_destination) == fixture_bytes);
        TAUREON_REQUIRE(manager.merge_frames({{first.value(), 0}, {duplicate.value(), 0}},
                                             merge_destination));
        TAUREON_REQUIRE(read_bytes(merge_destination).size() == fixture_bytes.size() * 2);
        TAUREON_REQUIRE(read_bytes(fixture) == fixture_bytes);

        for (const auto bytes : {64U * 1024U, 600U * 1024U, 900U * 1024U,
                                 1024U * 1024U + 257U}) {
            const auto input = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} /
                               ("stage5-manager-large-" + std::to_string(bytes) + ".syx");
            const auto output = std::filesystem::path{TAUREON_TEST_OUTPUT_DIR} /
                                ("stage5-manager-large-output-" + std::to_string(bytes) + ".syx");
            remove_file(input);
            remove_file(output);
            const auto original = large_frame(bytes);
            write_bytes(input, original);
            const auto large_item = manager.add_file(input);
            TAUREON_REQUIRE(large_item);
            TAUREON_REQUIRE(manager.snapshot().items.back().byte_count == original.size());
            TAUREON_REQUIRE(manager.export_frames(large_item.value(), {0}, output));
            TAUREON_REQUIRE(read_bytes(output) == original);
            remove_file(input);
            remove_file(output);
        }

        const auto large_snapshot = manager.snapshot();
        std::size_t observed_raw_bytes = 0;
        std::size_t observed_frames = 0;
        for (const auto& item : large_snapshot.items) {
            observed_raw_bytes += item.byte_count;
            observed_frames += item.frames.size();
        }
        TAUREON_REQUIRE(large_snapshot.items.size() == 6);
        TAUREON_REQUIRE(observed_frames == 6);
        TAUREON_REQUIRE(observed_raw_bytes == 2'651'423);

        write_bytes(replace_destination, {0x01, 0x02, 0x03});
        TAUREON_REQUIRE(!manager.export_frames(first.value(), {0}, replace_destination));
        TAUREON_REQUIRE(read_bytes(replace_destination) == std::vector<std::uint8_t>({0x01, 0x02, 0x03}));
        TAUREON_REQUIRE(manager.export_frames(first.value(), {0}, replace_destination, true));
        TAUREON_REQUIRE(read_bytes(replace_destination) == fixture_bytes);

        const auto missing_item = manager.export_frames(999'999, {0}, export_destination);
        TAUREON_REQUIRE(!missing_item);
        TAUREON_REQUIRE(missing_item.error().code == midi::MidiErrorCode::not_found);
        const auto empty_export = manager.export_frames(first.value(), {}, export_destination);
        TAUREON_REQUIRE(!empty_export);
        TAUREON_REQUIRE(empty_export.error().code == midi::MidiErrorCode::invalid_argument);
        const auto empty_merge = manager.merge_frames({}, merge_destination);
        TAUREON_REQUIRE(!empty_merge);
        TAUREON_REQUIRE(empty_merge.error().code == midi::MidiErrorCode::invalid_argument);
        const auto invalid_index = manager.export_frames(first.value(), {99}, export_destination);
        TAUREON_REQUIRE(!invalid_index);
        TAUREON_REQUIRE(invalid_index.error().code == midi::MidiErrorCode::invalid_argument);

        const auto failed_result = manager.export_frames(first.value(), {0}, failed_export);
        TAUREON_REQUIRE(!failed_result);
        TAUREON_REQUIRE(!std::filesystem::exists(failed_export));
        TAUREON_REQUIRE(!std::filesystem::exists(
            std::filesystem::path{failed_export.string() + ".taureon.tmp"}));

        write_bytes(incomplete_path, {0xF0, 0x7D, 0x22});
        const auto incomplete = manager.add_file(incomplete_path);
        TAUREON_REQUIRE(incomplete);
        const auto incomplete_snapshot = manager.snapshot().items.back();
        TAUREON_REQUIRE(incomplete_snapshot.incomplete_frames == 1);
        TAUREON_REQUIRE(incomplete_snapshot.malformed_frames == 0);
        TAUREON_REQUIRE(!manager.can_transfer(incomplete.value()));
        TAUREON_REQUIRE(!manager.transferable_item(incomplete.value()));
        TAUREON_REQUIRE(!manager.export_frames(incomplete.value(), {0}, export_destination));

        write_bytes(malformed_path, {0xF0, 0x7D, 0x80});
        const auto malformed = manager.add_file(malformed_path);
        TAUREON_REQUIRE(malformed);
        const auto malformed_snapshot = manager.snapshot().items.back();
        TAUREON_REQUIRE(malformed_snapshot.incomplete_frames == 0);
        TAUREON_REQUIRE(malformed_snapshot.malformed_frames == 1);
        TAUREON_REQUIRE(!manager.can_transfer(malformed.value()));
        TAUREON_REQUIRE(!manager.merge_frames({{malformed.value(), 0}}, merge_destination));

        sysex::SyxDocument tainted_document;
        tainted_document.raw_bytes = {0xF0, 0x7D, 0x44, 0xF7};
        tainted_document.frames.push_back(
            {sysex::SysExFrameStatus::complete, tainted_document.raw_bytes, "injected data loss",
             std::nullopt, true});
        const auto tainted = manager.add_document(std::move(tainted_document), "captured-tainted.syx");
        TAUREON_REQUIRE(tainted);
        const auto tainted_snapshot = manager.snapshot().items.back();
        TAUREON_REQUIRE(tainted_snapshot.tainted_frames == 1);
        TAUREON_REQUIRE(!manager.can_transfer(tainted.value()));
        TAUREON_REQUIRE(!manager.transferable_item(tainted.value()));
        TAUREON_REQUIRE(!manager.export_frames(tainted.value(), {0}, export_destination));
        TAUREON_REQUIRE(!manager.merge_frames({{tainted.value(), 0}}, merge_destination));

        write_bytes(first_distinct_path, {0xF0, 0x7D, 0x01, 0xF7});
        write_bytes(second_distinct_path, {0xF0, 0x7D, 0x02, 0xF7});
        app::SysExManager distinct_manager;
        const auto distinct_first = distinct_manager.add_file(first_distinct_path);
        const auto distinct_second = distinct_manager.add_file(second_distinct_path);
        TAUREON_REQUIRE(distinct_first && distinct_second);
        const auto distinct_snapshot = distinct_manager.snapshot();
        TAUREON_REQUIRE(!distinct_snapshot.items.at(1).exact_file_duplicate_of);
        TAUREON_REQUIRE(distinct_snapshot.items.at(0).frames.front().exact_frame_duplicates.empty());
        TAUREON_REQUIRE(distinct_snapshot.items.at(0).frames.front().exact_payload_duplicates.empty());
        TAUREON_REQUIRE(distinct_snapshot.items.at(1).frames.front().exact_frame_duplicates.empty());
        TAUREON_REQUIRE(distinct_snapshot.items.at(1).frames.front().exact_payload_duplicates.empty());

        TAUREON_REQUIRE(distinct_manager.merge_frames(
            {{distinct_second.value(), 0}, {distinct_first.value(), 0}}, merge_destination, true));
        TAUREON_REQUIRE(read_bytes(merge_destination) ==
                        std::vector<std::uint8_t>({0xF0, 0x7D, 0x02, 0xF7,
                                                   0xF0, 0x7D, 0x01, 0xF7}));

        TAUREON_REQUIRE(manager.remove_item(duplicate.value()));
        TAUREON_REQUIRE(read_bytes(fixture) == fixture_bytes);

        remove_file(export_destination);
        remove_file(merge_destination);
        remove_file(replace_destination);
        remove_file(incomplete_path);
        remove_file(malformed_path);
        remove_file(first_distinct_path);
        remove_file(second_distinct_path);
        remove_file(oversized_path);
        std::filesystem::remove_all(no_output_parent, ignored);
    });
}
