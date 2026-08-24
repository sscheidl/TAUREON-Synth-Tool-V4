#pragma once

#include "SysExFrame.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace taureon::sysex {

struct SysExParseBatch {
    std::vector<SysExFrame> frames;
    std::vector<std::uint8_t> realtime;
};

class SysExStreamParser {
public:
    [[nodiscard]] SysExParseBatch consume(std::span<const std::uint8_t> bytes);
    [[nodiscard]] std::vector<SysExFrame> finish();
    void reset() noexcept;
    void notify_data_loss() noexcept;

    [[nodiscard]] bool inside_frame() const noexcept;
    [[nodiscard]] const SysExParserDiagnostics& diagnostics() const noexcept;

private:
    void record(const SysExFrame& frame) noexcept;

    std::vector<std::uint8_t> current_;
    bool inside_{};
    bool current_data_loss_{};
    bool pending_data_loss_{};
    SysExParserDiagnostics diagnostics_;
};

} // namespace taureon::sysex
