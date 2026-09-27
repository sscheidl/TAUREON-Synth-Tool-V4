#include "gui/MidiMonitorFilterModel.hpp"

#include "gui/MidiMonitorModel.hpp"

#include <initializer_list>
#include <utility>

namespace taureon::gui {

MidiMonitorFilterModel::MidiMonitorFilterModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
}

void MidiMonitorFilterModel::set_direction(QString direction) {
    beginFilterChange();
    direction_ = std::move(direction);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void MidiMonitorFilterModel::set_type_filter(QString type_filter) {
    beginFilterChange();
    type_filter_ = std::move(type_filter);
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

bool MidiMonitorFilterModel::filterAcceptsRow(const int source_row,
                                               const QModelIndex& source_parent) const {
    const auto direction = sourceModel()->index(source_row, MidiMonitorModel::Direction, source_parent)
                               .data(Qt::DisplayRole).toString();
    if (!direction_.isEmpty() && direction != direction_) return false;
    if (type_filter_.isEmpty()) return true;
    for (const auto column : {MidiMonitorModel::Type, MidiMonitorModel::Event, MidiMonitorModel::Value}) {
        if (sourceModel()->index(source_row, column, source_parent)
                .data(Qt::DisplayRole).toString().contains(type_filter_, Qt::CaseInsensitive)) return true;
    }
    return false;
}

} // namespace taureon::gui
