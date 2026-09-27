#include "gui/MidiMonitorModel.hpp"

#include "core/midi/MidiMessage.hpp"

#include <QStringList>

#include <algorithm>
#include <array>
#include <optional>
#include <stdexcept>
#include <variant>

namespace taureon::gui {
namespace {

QString hex_bytes(const std::vector<std::uint8_t>& bytes) {
    QStringList values;
    values.reserve(static_cast<qsizetype>(bytes.size()));
    for (const auto byte : bytes) values.push_back(QStringLiteral("%1").arg(byte, 2, 16, QLatin1Char('0')));
    return values.join(' ').toUpper();
}

std::optional<midi::ParsedMidi1Message> channel_voice_message(const midi::NativeMidiMessage& message) {
    if (const auto* midi1 = std::get_if<midi::Midi1NativeMessage>(&message.data)) {
        if (midi1->bytes.size() < 2 || midi1->bytes.size() > 3 ||
            midi1->bytes.front() < 0x80 || midi1->bytes.front() >= 0xf0) {
            return std::nullopt;
        }
        const auto parsed = midi::parse_midi1_message(midi1->bytes);
        return parsed ? std::optional{parsed.value()} : std::nullopt;
    }
    const auto& words = std::get<midi::UmpNativeMessage>(message.data).words;
    if (words.size() != 1 || (words.front() >> 28u) != 0x2u) return std::nullopt;
    const auto word = words.front();
    const auto status = static_cast<std::uint8_t>((word >> 16u) & 0xffu);
    if (status < 0x80 || status >= 0xf0) return std::nullopt;
    const std::array bytes{status, static_cast<std::uint8_t>((word >> 8u) & 0xffu),
                           static_cast<std::uint8_t>(word & 0xffu)};
    const auto size = (status & 0xf0u) == 0xc0u || (status & 0xf0u) == 0xd0u ? 2u : 3u;
    const auto parsed = midi::parse_midi1_message(std::span{bytes.data(), size});
    return parsed ? std::optional{parsed.value()} : std::nullopt;
}

QString channel_voice_type(const midi::Midi1MessageKind kind) {
    switch (kind) {
    case midi::Midi1MessageKind::note_on: return "Note On";
    case midi::Midi1MessageKind::note_off: return "Note Off";
    case midi::Midi1MessageKind::polyphonic_aftertouch: return "Poly Pressure";
    case midi::Midi1MessageKind::control_change: return "Control Change";
    case midi::Midi1MessageKind::program_change: return "Program Change";
    case midi::Midi1MessageKind::channel_pressure: return "Channel Pressure";
    case midi::Midi1MessageKind::pitch_bend: return "Pitch Bend";
    default: return "MIDI 1.0";
    }
}

QString channel_voice_event(const midi::ParsedMidi1Message& parsed) {
    switch (parsed.kind) {
    case midi::Midi1MessageKind::note_on:
    case midi::Midi1MessageKind::note_off:
    case midi::Midi1MessageKind::polyphonic_aftertouch:
        return QStringLiteral("Note %1").arg(*parsed.data1);
    case midi::Midi1MessageKind::control_change:
        return QStringLiteral("CC %1").arg(*parsed.data1);
    case midi::Midi1MessageKind::program_change:
        return QStringLiteral("Program %1").arg(*parsed.data1);
    case midi::Midi1MessageKind::channel_pressure: return "Channel";
    case midi::Midi1MessageKind::pitch_bend: return "Pitch wheel";
    default: return "—";
    }
}

QVariant channel_voice_value(const midi::ParsedMidi1Message& parsed) {
    if (parsed.kind == midi::Midi1MessageKind::pitch_bend) return static_cast<int>(*parsed.value14);
    if (parsed.kind == midi::Midi1MessageKind::channel_pressure) return static_cast<int>(*parsed.data1);
    if (parsed.data2) return static_cast<int>(*parsed.data2);
    return QStringLiteral("—");
}

QString message_type(const midi::NativeMidiMessage& message) {
    if (const auto* midi1 = std::get_if<midi::Midi1NativeMessage>(&message.data)) {
        const auto parsed = midi::parse_midi1_message(midi1->bytes);
        if (!parsed) return "Malformed MIDI 1.0";
        switch (parsed.value().kind) {
        case midi::Midi1MessageKind::note_on: return "Note On";
        case midi::Midi1MessageKind::note_off: return "Note Off";
        case midi::Midi1MessageKind::control_change: return "Control Change";
        case midi::Midi1MessageKind::system_exclusive: return "SysEx";
        case midi::Midi1MessageKind::timing_clock: return "Clock";
        case midi::Midi1MessageKind::active_sensing: return "Active Sensing";
        default: return "MIDI 1.0";
        }
    }
    return "UMP";
}

} // namespace

MidiMonitorModel::MidiMonitorModel(const std::size_t history_limit, QObject* parent)
    : QAbstractTableModel(parent), history_limit_(history_limit) {
    if (history_limit == 0) throw std::invalid_argument("monitor history limit must be positive");
}

int MidiMonitorModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(events_.size());
}

int MidiMonitorModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant MidiMonitorModel::data(const QModelIndex& index, const int role) const {
    if (!index.isValid() || role != Qt::DisplayRole || index.row() >= rowCount()) return {};
    const auto& event = events_.at(static_cast<std::size_t>(index.row()));
    const auto& message = event.message;
    const auto* midi1 = std::get_if<midi::Midi1NativeMessage>(&message.data);
    const auto parsed_voice = index.column() >= Channel && index.column() <= Value
                                  ? channel_voice_message(message) : std::nullopt;
    switch (index.column()) {
    case Time: return message.timestamp ? QString::number(message.timestamp->native_value) : QStringLiteral("—");
    case Direction: return event.direction == midi::MidiDirection::input ? "RX" : "TX";
    case Route: return QString::fromLatin1(midi::to_string(message.backend));
    case Channel:
        return parsed_voice ? QVariant{static_cast<int>(*parsed_voice->channel + 1)} : QVariant{QStringLiteral("—")};
    case Type: return parsed_voice ? channel_voice_type(parsed_voice->kind) : message_type(message);
    case Event: return parsed_voice ? channel_voice_event(*parsed_voice) : QStringLiteral("—");
    case Value: return parsed_voice ? channel_voice_value(*parsed_voice) : QVariant{QStringLiteral("—")};
    case Raw: return midi1 ? hex_bytes(midi1->bytes) : QStringLiteral("UMP words");
    default: return {};
    }
}

QVariant MidiMonitorModel::headerData(const int section, const Qt::Orientation orientation,
                                      const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= ColumnCount) return {};
    static const QStringList headers{"Time", "Direction", "Route", "Channel", "Type", "Event", "Value", "Raw"};
    return headers.at(section);
}

void MidiMonitorModel::append_batch(std::vector<app::MonitorEvent> events) {
    if (events.empty()) return;
    if (events.size() >= history_limit_) {
        events.erase(events.begin(), events.end() - static_cast<std::ptrdiff_t>(history_limit_));
        beginResetModel();
        events_.assign(std::make_move_iterator(events.begin()), std::make_move_iterator(events.end()));
        endResetModel();
        return;
    }
    const auto overflow = events_.size() + events.size() > history_limit_ ?
        events_.size() + events.size() - history_limit_ : 0;
    if (overflow > 0) {
        beginRemoveRows({}, 0, static_cast<int>(overflow - 1));
        for (std::size_t index = 0; index < overflow; ++index) events_.pop_front();
        endRemoveRows();
    }
    const auto first = static_cast<int>(events_.size());
    beginInsertRows({}, first, first + static_cast<int>(events.size()) - 1);
    for (auto& event : events) events_.push_back(std::move(event));
    endInsertRows();
}

void MidiMonitorModel::clear() {
    if (events_.empty()) return;
    beginResetModel();
    events_.clear();
    endResetModel();
}

} // namespace taureon::gui
