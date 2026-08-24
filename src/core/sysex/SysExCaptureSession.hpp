#pragma once

#include "SysEx7.hpp"
#include "SysExStreamParser.hpp"
#include "core/midi/MidiTypes.hpp"

#include <vector>

namespace taureon::sysex {

class SysExCaptureSession {
public:
    [[nodiscard]] std::vector<SysExFrame> consume(const midi::MidiStreamEvent& event);
    [[nodiscard]] std::vector<SysExFrame> finish();
    void reset() noexcept;

private:
    SysExStreamParser midi1_parser_;
    SysEx7Assembler sysex7_assembler_;
};

} // namespace taureon::sysex
