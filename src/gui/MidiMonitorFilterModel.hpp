#pragma once

#include "gui/MidiMonitorModel.hpp"

#include <QSortFilterProxyModel>

#include <cstdint>

namespace taureon::gui {

class MidiMonitorFilterModel final : public QSortFilterProxyModel {
public:
    explicit MidiMonitorFilterModel(QObject* parent = nullptr);

    void set_direction(QString direction);
    void set_route(QString route);
    void set_channel(int channel);
    void set_type_filter(QString type_filter);
    void set_category_enabled(MonitorEventCategory category, bool enabled);
    [[nodiscard]] bool category_enabled(MonitorEventCategory category) const noexcept;

protected:
    [[nodiscard]] bool filterAcceptsRow(int source_row,
                                        const QModelIndex& source_parent) const override;

private:
    QString direction_;
    QString route_;
    int channel_{};
    QString type_filter_;
    std::uint32_t category_mask_{(1u << static_cast<unsigned>(MonitorEventCategory::count)) - 1u};
};

} // namespace taureon::gui
