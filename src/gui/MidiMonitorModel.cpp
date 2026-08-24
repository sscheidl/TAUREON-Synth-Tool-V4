#include "gui/MidiMonitorModel.hpp"

#include "core/midi/MidiMessage.hpp"

#include <QStringList>

#include <algorithm>
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
    switch (index.column()) {
    case Time: return message.timestamp ? QString::number(message.timestamp->native_value) : QStringLiteral("—");
    case Direction: return event.direction == midi::MidiDirection::input ? "RX" : "TX";
    case Route: return QString::fromLatin1(midi::to_string(message.backend));
    case Group: return QStringLiteral("—");
    case Channel:
        if (midi1) {
            const auto parsed = midi::parse_midi1_message(midi1->bytes);
            if (parsed && parsed.value().channel) return static_cast<int>(*parsed.value().channel + 1);
        }
        return QStringLiteral("—");
    case Type: return message_type(message);
    case Event: return QStringLiteral("—");
    case Value: return QStringLiteral("—");
    case Raw: return midi1 ? hex_bytes(midi1->bytes) : QStringLiteral("UMP words");
    default: return {};
    }
}

QVariant MidiMonitorModel::headerData(const int section, const Qt::Orientation orientation,
                                      const int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole || section < 0 || section >= ColumnCount) return {};
    static const QStringList headers{"Time", "Direction", "Route", "Group", "Channel", "Type", "Event", "Value", "Raw"};
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
