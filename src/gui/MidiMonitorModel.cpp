#include "gui/MidiMonitorModel.hpp"

#include <taureon/core/midi/MidiMessage.hpp>

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

QString hex_words(const std::vector<std::uint32_t>& words) {
    QStringList values;
    values.reserve(static_cast<qsizetype>(words.size()));
    for (const auto word : words) values.push_back(QStringLiteral("%1").arg(word, 8, 16, QLatin1Char('0')));
    return values.join(' ').toUpper();
}

struct Midi2VoiceDetail {
    int channel;
    QString type;
    QString event;
    QString value;
};

std::optional<Midi2VoiceDetail> midi2_voice_detail(const midi::NativeMidiMessage& message) {
    const auto* ump = std::get_if<midi::UmpNativeMessage>(&message.data);
    if (!ump || ump->words.size() != 2) return std::nullopt;
    const auto first = ump->words[0];
    if ((first >> 28u) != 0x4u) return std::nullopt;
    const auto status = (first >> 20u) & 0xfu;
    const auto channel = static_cast<int>(((first >> 16u) & 0xfu) + 1u);
    const auto index = (first >> 8u) & 0xffu;
    const auto data = ump->words[1];
    switch (status) {
    case 0x8u:
    case 0x9u:
        if (index > 127u) return std::nullopt;
        return Midi2VoiceDetail{channel, status == 0x8u ? "MIDI 2.0 Note Off" : "MIDI 2.0 Note On",
                                QStringLiteral("Note %1").arg(index), QString::number(data >> 16u)};
    case 0xau:
        if (index > 127u) return std::nullopt;
        return Midi2VoiceDetail{channel, "MIDI 2.0 Poly Pressure", QStringLiteral("Note %1").arg(index),
                                QString::number(data)};
    case 0xbu:
        if (index > 127u) return std::nullopt;
        return Midi2VoiceDetail{channel, "MIDI 2.0 Control Change", QStringLiteral("CC %1").arg(index),
                                QString::number(data)};
    case 0xdu:
        return Midi2VoiceDetail{channel, "MIDI 2.0 Channel Pressure", "Channel", QString::number(data)};
    case 0xeu:
        return Midi2VoiceDetail{channel, "MIDI 2.0 Pitch Bend", "Pitch wheel", QString::number(data)};
    default: return std::nullopt;
    }
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
    const auto& words = std::get<midi::UmpNativeMessage>(message.data).words;
    if (words.empty()) return "Malformed UMP";
    const auto word = words.front();
    const auto ump_type = word >> 28u;
    if (ump_type == 0x3u) return "SysEx7";
    if (ump_type == 0x5u && ((word >> 20u) & 0xfu) <= 0x3u) return "SysEx8";
    if (ump_type == 0x1u) {
        switch ((word >> 16u) & 0xffu) {
        case 0xf8u: return "Clock";
        case 0xfeu: return "Active Sensing";
        default: break;
        }
    }
    return "UMP";
}

MonitorEventCategory channel_category(const std::uint8_t status) {
    switch (status & 0xf0u) {
    case 0x80u:
    case 0x90u: return MonitorEventCategory::notes;
    case 0xa0u:
    case 0xd0u: return MonitorEventCategory::aftertouch;
    case 0xb0u: return MonitorEventCategory::controllers;
    case 0xc0u: return MonitorEventCategory::program_change;
    case 0xe0u: return MonitorEventCategory::pitch_bend;
    default: return MonitorEventCategory::other;
    }
}

MonitorEventCategory system_category(const std::uint8_t status) {
    switch (status) {
    case 0xf0u:
    case 0xf7u: return MonitorEventCategory::sysex;
    case 0xf8u: return MonitorEventCategory::clock;
    case 0xfeu: return MonitorEventCategory::active_sensing;
    default: return MonitorEventCategory::other;
    }
}

MonitorEventCategory event_category(const midi::NativeMidiMessage& message) {
    if (const auto* midi1 = std::get_if<midi::Midi1NativeMessage>(&message.data)) {
        if (midi1->bytes.empty()) return MonitorEventCategory::other;
        const auto status = midi1->bytes.front();
        return status < 0xf0u ? channel_category(status) : system_category(status);
    }
    const auto& words = std::get<midi::UmpNativeMessage>(message.data).words;
    if (words.empty()) return MonitorEventCategory::other;
    const auto word = words.front();
    const auto message_type = static_cast<std::uint8_t>(word >> 28u);
    if (message_type == 0x3u ||
        (message_type == 0x5u && ((word >> 20u) & 0xfu) <= 0x3u)) {
        return MonitorEventCategory::sysex;
    }
    if (message_type == 0x1u) return system_category(static_cast<std::uint8_t>(word >> 16u));
    if (message_type == 0x2u || message_type == 0x4u) {
        return channel_category(static_cast<std::uint8_t>(word >> 16u));
    }
    return MonitorEventCategory::other;
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
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const auto& event = events_.at(static_cast<std::size_t>(index.row()));
    const auto& message = event.message;
    if (role == CategoryRole) return static_cast<int>(event_category(message));
    if (role != Qt::DisplayRole) return {};
    const auto* midi1 = std::get_if<midi::Midi1NativeMessage>(&message.data);
    const auto parsed_voice = index.column() >= Channel && index.column() <= Value
                                  ? channel_voice_message(message) : std::nullopt;
    const auto midi2_voice = index.column() >= Channel && index.column() <= Value
                                 ? midi2_voice_detail(message) : std::nullopt;
    switch (index.column()) {
    case Time: return message.timestamp ? QString::number(message.timestamp->native_value) : QStringLiteral("—");
    case Direction: return event.direction == midi::MidiDirection::input ? "RX" : "TX";
    case Route: return QString::fromLatin1(midi::to_string(message.backend));
    case Channel:
        if (parsed_voice) return static_cast<int>(*parsed_voice->channel + 1);
        return midi2_voice ? QVariant{midi2_voice->channel} : QVariant{QStringLiteral("—")};
    case Type:
        if (parsed_voice) return channel_voice_type(parsed_voice->kind);
        return midi2_voice ? midi2_voice->type : message_type(message);
    case Event:
        if (parsed_voice) return channel_voice_event(*parsed_voice);
        return midi2_voice ? midi2_voice->event : QStringLiteral("—");
    case Value:
        if (parsed_voice) return channel_voice_value(*parsed_voice);
        return midi2_voice ? midi2_voice->value : QStringLiteral("—");
    case Raw: return midi1 ? hex_bytes(midi1->bytes) : hex_words(std::get<midi::UmpNativeMessage>(message.data).words);
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

void MidiMonitorModel::set_history_limit(const std::size_t history_limit) {
    if (history_limit == 0) throw std::invalid_argument("monitor history limit must be positive");
    history_limit_ = history_limit;
    if (events_.size() <= history_limit_) return;
    const auto excess = events_.size() - history_limit_;
    beginRemoveRows({}, 0, static_cast<int>(excess - 1));
    for (std::size_t index = 0; index < excess; ++index) events_.pop_front();
    endRemoveRows();
}

} // namespace taureon::gui
