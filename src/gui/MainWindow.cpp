#include "gui/MainWindow.hpp"
#include "gui/LibrarianPanel.hpp"
#include "app/Diagnostics.hpp"
#include "gui/DiagnosticsPanel.hpp"
#include "gui/SettingsPanel.hpp"
#include "gui/MidiMonitorModel.hpp"
#include "gui/MidiMonitorFilterModel.hpp"
#include "gui/MonitorEventBridge.hpp"
#include "gui/ProfileMatchPanel.hpp"
#include "gui/SysExTransferPanel.hpp"
#include "gui/SysExManagerPanel.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCheckBox>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStandardPaths>
#include <QStyle>
#include <QStringList>
#include <QToolBar>
#include <QToolButton>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace taureon::gui {
namespace {

constexpr auto kWorkspaceNames = std::array{
    "MIDI Monitor",
    "SysEx Transfer",
    "SysEx Manager",
    "Library / Librarian",
    "Devices & Profiles",
    "Diagnostics",
    "Settings",
};

QLabel* add_caption(QToolBar& bar, const QString& caption, const int horizontal_padding = 0) {
    auto* label = new QLabel(caption, &bar);
    label->setObjectName("connectionCaption");
    label->setContentsMargins(horizontal_padding, 0, horizontal_padding, 0);
    bar.addWidget(label);
    return label;
}

QComboBox* add_selector(QToolBar& bar, const QStringList& values) {
    auto* selector = new QComboBox(&bar);
    selector->addItems(values);
    bar.addWidget(selector);
    return selector;
}

class RouteSelector final : public QComboBox {
public:
    explicit RouteSelector(QWidget* parent) : QComboBox(parent) {
        setSizeAdjustPolicy(AdjustToMinimumContentsLengthWithIcon);
        setMinimumContentsLength(18);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(this, &QComboBox::currentIndexChanged, this, [this](int index) {
            const auto detail = itemData(index, Qt::ToolTipRole).toString();
            setToolTip(detail.isEmpty() ? currentText() : detail);
        });
    }

    void showPopup() override {
        int content_width = width();
        for (int index = 0; index < count(); ++index) {
            content_width = (std::max)(content_width,
                fontMetrics().horizontalAdvance(itemText(index)) + 48);
        }
        const int screen_width = screen()->availableGeometry().width();
        view()->setMinimumWidth((std::min)(content_width, (std::max)(width(), screen_width - 32)));
        view()->setTextElideMode(Qt::ElideRight);
        QComboBox::showPopup();
    }
};

QString endpoint_label(const midi::MidiEndpointDescriptor& endpoint) {
    return std::visit(
        [&endpoint](const auto& identity) -> QString {
            using Identity = std::decay_t<decltype(identity)>;
            if constexpr (std::is_same_v<Identity, midi::WmsRouteIdentity>) {
                return QStringLiteral("%1 — %2 · group %3")
                    .arg(QString::fromStdString(endpoint.display_name),
                         QString::fromStdString(identity.endpoint_device_id))
                    .arg(identity.group + 1);
            } else if constexpr (std::is_same_v<Identity, midi::WinmmRouteIdentity>) {
                return QStringLiteral("%1 — manufacturer %2 · product %3 · driver %4")
                    .arg(QString::fromStdString(endpoint.display_name))
                    .arg(identity.manufacturer_id)
                    .arg(identity.product_id)
                    .arg(identity.driver_version);
            } else {
                return QStringLiteral("%1 — %2 · %3")
                    .arg(QString::fromStdString(endpoint.display_name),
                         QString::fromStdString(identity.provider_id),
                         QString::fromStdString(identity.endpoint_id));
            }
        }, endpoint.identity.native);
}

QString endpoint_name(const midi::MidiEndpointDescriptor& endpoint) {
    const auto name = QString::fromStdString(endpoint.display_name);
    if (const auto* wms = std::get_if<midi::WmsRouteIdentity>(&endpoint.identity.native)) {
        return QStringLiteral("%1 · group %2").arg(name).arg(wms->group + 1);
    }
    return name;
}

QWidget* make_workspace_page(const QString& name) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* title = new QLabel(name, page);
    title->setObjectName("workspaceTitle");
    auto* message = new QLabel(
        "This workspace is being connected in a later Stage 5 vertical slice. "
        "No MIDI route, file, profile binding, or transfer is changed from this shell.", page);
    message->setWordWrap(true);
    message->setObjectName("workspaceUnavailableMessage");
    layout->addWidget(title);
    layout->addWidget(message);
    layout->addStretch();
    return page;
}

QString monitor_delimited_line(const QAbstractItemModel& model, const int row, const QChar separator) {
    const auto cell = [separator](QString value) {
        if (value.contains('"') || value.contains(separator) || value.contains('\n') || value.contains('\r')) {
            value.replace('"', "\"\"");
            return QStringLiteral("\"") + value + QStringLiteral("\"");
        }
        return value;
    };
    QStringList fields;
    for (int column = 0; column < model.columnCount(); ++column) {
        fields.push_back(cell(row < 0 ? model.headerData(column, Qt::Horizontal).toString()
                                      : model.index(row, column).data(Qt::DisplayRole).toString()));
    }
    return fields.join(separator);
}

QWidget* make_monitor_page(MidiMonitorModel& model, MonitorEventBridge& bridge,
                           QPushButton*& pause_button) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* controls = new QHBoxLayout;
    auto* direction = new QComboBox(page);
    direction->addItems({"All directions", "RX", "TX"});
    direction->setAccessibleName("Monitor direction filter");
    auto* type_filter = new QLineEdit(page);
    type_filter->setPlaceholderText("Filter type, event or value");
    type_filter->setAccessibleName("Monitor type, event or value filter");
    auto* pause = new QPushButton("Pause presentation", page);
    pause->setCheckable(true);
    pause_button = pause;
    pause->setToolTip("Presentation events are counted and discarded while paused; transport capture continues.");
    auto* clear = new QPushButton("Clear", page);
    auto* follow = new QCheckBox("Follow latest", page);
    follow->setObjectName("monitorFollowLatest");
    follow->setChecked(true);
    follow->setToolTip("Follow new events. Scrolling up pauses following; check this to resume.");
    auto* accounting = new QLabel("Presentation running", page);
    accounting->setObjectName("monitorPresentationAccounting");
    controls->addWidget(direction);
    controls->addWidget(type_filter, 1);
    controls->addWidget(pause);
    controls->addWidget(clear);
    controls->addWidget(follow);
    controls->addWidget(accounting);
    layout->addLayout(controls);

    auto* proxy = new MidiMonitorFilterModel(page);
    proxy->setSourceModel(&model);
    auto* filter_controls = new QHBoxLayout;
    auto* event_types = new QToolButton(page);
    event_types->setAccessibleName("Monitor event type filter");
    event_types->setPopupMode(QToolButton::InstantPopup);
    auto* event_menu = new QMenu(event_types);
    event_types->setMenu(event_menu);
    const auto categories = std::array{
        std::pair{MonitorEventCategory::notes, "Notes"},
        std::pair{MonitorEventCategory::controllers, "Control Change"},
        std::pair{MonitorEventCategory::program_change, "Program Change"},
        std::pair{MonitorEventCategory::pitch_bend, "Pitch Bend"},
        std::pair{MonitorEventCategory::aftertouch, "Aftertouch"},
        std::pair{MonitorEventCategory::sysex, "SysEx"},
        std::pair{MonitorEventCategory::clock, "Clock"},
        std::pair{MonitorEventCategory::active_sensing, "Active Sensing"},
        std::pair{MonitorEventCategory::other, "Other / System"},
    };
    auto* show_all = event_menu->addAction("Show all event types");
    event_menu->addSeparator();
    auto category_actions = std::make_shared<std::vector<QAction*>>();
    for (const auto& [category, name] : categories) {
        auto* action = event_menu->addAction(name);
        action->setCheckable(true);
        action->setChecked(true);
        category_actions->push_back(action);
        QObject::connect(action, &QAction::toggled, page,
                         [proxy, category, event_types, category_actions](const bool checked) {
            proxy->set_category_enabled(category, checked);
            const auto selected = std::count_if(category_actions->begin(), category_actions->end(),
                                                [](const QAction* item) { return item->isChecked(); });
            event_types->setText(selected == static_cast<int>(category_actions->size())
                                     ? "Event types: All"
                                     : QStringLiteral("Event types: %1/%2")
                                           .arg(selected).arg(category_actions->size()));
        });
    }
    event_types->setText("Event types: All");
    const auto label_width = std::max(event_types->fontMetrics().horizontalAdvance("Event types: All"),
                                      event_types->fontMetrics().horizontalAdvance("Event types: 0/9"));
    const auto arrow_width = event_types->style()->pixelMetric(QStyle::PM_MenuButtonIndicator,
                                                               nullptr, event_types);
    const auto margin = event_types->style()->pixelMetric(QStyle::PM_ButtonMargin,
                                                          nullptr, event_types);
    event_types->setMinimumWidth(label_width + arrow_width + 2 * margin + 12);
    QObject::connect(show_all, &QAction::triggered, page, [category_actions] {
        for (auto* action : *category_actions) action->setChecked(true);
    });
    auto* channel = new QComboBox(page);
    channel->setAccessibleName("Monitor channel filter");
    channel->addItem("All channels", 0);
    for (int value = 1; value <= 16; ++value) channel->addItem(QStringLiteral("Channel %1").arg(value), value);
    auto* backend_filter = new QComboBox(page);
    backend_filter->setAccessibleName("Monitor backend filter");
    backend_filter->addItem("All backends", QString{});
    backend_filter->addItem("Windows MIDI Services", "wms");
    backend_filter->addItem("WinMM", "winmm");
    backend_filter->addItem("External", "external");
    filter_controls->addWidget(event_types);
    filter_controls->addWidget(channel);
    filter_controls->addWidget(backend_filter);
    filter_controls->addStretch();
    layout->addLayout(filter_controls);
    auto* table = new QTableView(page);
    table->setObjectName("midiMonitorTable");
    table->setModel(proxy);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSortingEnabled(false);
    table->setContextMenuPolicy(Qt::ActionsContextMenu);
    table->setToolTip("Right-click to copy selected rows or export all visible rows.");
    auto* copy_action = new QAction("Copy selected rows", table);
    copy_action->setShortcut(QKeySequence::Copy);
    copy_action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    table->addAction(copy_action);
    QObject::connect(copy_action, &QAction::triggered, table, [table, proxy] {
        std::vector<int> rows;
        for (const auto& index : table->selectionModel()->selectedRows()) rows.push_back(index.row());
        if (rows.empty()) return;
        std::sort(rows.begin(), rows.end());
        QStringList lines;
        for (const auto row : rows) lines.push_back(monitor_delimited_line(*proxy, row, '\t'));
        QApplication::clipboard()->setText(lines.join("\r\n"));
    });
    auto* export_action = new QAction("Export visible rows as CSV…", table);
    table->addAction(export_action);
    QObject::connect(export_action, &QAction::triggered, table, [table, proxy] {
        const auto selected = QFileDialog::getSaveFileName(table, "Export MIDI Monitor", "midi-monitor.csv",
                                                       "CSV files (*.csv)");
        if (selected.isEmpty()) return;
        QSaveFile output(selected);
        bool saved = output.open(QIODevice::WriteOnly);
        const auto write_line = [&output](const QString& line) {
            const auto bytes = (line + "\r\n").toUtf8();
            return output.write(bytes) == bytes.size();
        };
        if (saved) saved = write_line(monitor_delimited_line(*proxy, -1, ','));
        for (int row = 0; saved && row < proxy->rowCount(); ++row) {
            saved = write_line(monitor_delimited_line(*proxy, row, ','));
        }
        if (saved) saved = output.commit();
        if (!saved) {
            QMessageBox::warning(table, "Export MIDI Monitor", "The selected CSV file could not be saved.");
        }
    });
    layout->addWidget(table);

    // Coalesce a burst into one scroll after Qt has updated the view geometry. This also
    // follows resets/evictions at the bounded history limit and changes to the visible filter.
    auto scroll_queued = std::make_shared<bool>(false);
    const auto follow_latest = [table, follow, scroll_queued] {
        if (!follow->isChecked() || *scroll_queued) return;
        *scroll_queued = true;
        QTimer::singleShot(0, table, [table, follow, scroll_queued] {
            *scroll_queued = false;
            if (follow->isChecked()) table->scrollToBottom();
        });
    };
    QObject::connect(proxy, &QAbstractItemModel::rowsInserted, table, follow_latest);
    QObject::connect(proxy, &QAbstractItemModel::modelReset, table, follow_latest);
    QObject::connect(proxy, &QAbstractItemModel::layoutChanged, table, follow_latest);
    QObject::connect(follow, &QCheckBox::toggled, table, follow_latest);
    auto* vertical_scroll = table->verticalScrollBar();
    QObject::connect(vertical_scroll, &QScrollBar::actionTriggered, table,
                     [vertical_scroll, follow] {
        follow->setChecked(vertical_scroll->sliderPosition() >= vertical_scroll->maximum());
    });
    QObject::connect(vertical_scroll, &QScrollBar::sliderMoved, table,
                     [vertical_scroll, follow](int position) {
        follow->setChecked(position >= vertical_scroll->maximum());
    });

    QObject::connect(direction, &QComboBox::currentTextChanged, page,
                     [proxy](const QString& value) {
                         proxy->set_direction(value == "All directions" ? QString{} : value);
                     });
    QObject::connect(channel, &QComboBox::currentIndexChanged, page,
                     [proxy, channel] { proxy->set_channel(channel->currentData().toInt()); });
    QObject::connect(backend_filter, &QComboBox::currentIndexChanged, page,
                     [proxy, backend_filter] { proxy->set_backend(backend_filter->currentData().toString()); });
    QObject::connect(type_filter, &QLineEdit::textChanged, page,
                     [proxy](const QString& value) { proxy->set_type_filter(value); });
    QObject::connect(pause, &QPushButton::toggled, page, [&bridge, pause](const bool paused) {
        bridge.set_paused(paused);
        pause->setText(paused ? "Resume presentation" : "Pause presentation");
    });
    QObject::connect(clear, &QPushButton::clicked, page, [&model] { model.clear(); });
    auto* accounting_timer = new QTimer(page);
    accounting_timer->setInterval(250);
    QObject::connect(accounting_timer, &QTimer::timeout, page, [&bridge, accounting, &model, proxy] {
        const auto stats = bridge.presentation_stats();
        accounting->setText(
            QStringLiteral("%1 · visible %2/%3 · presented %4 · paused-discarded %5")
                .arg(stats.paused ? "Paused" : "Running")
                .arg(proxy->rowCount())
                .arg(model.rowCount())
                .arg(stats.displayed)
                .arg(stats.discarded_while_paused));
    });
    accounting_timer->start();
    return page;
}

} // namespace

MainWindow::MainWindow(app::MonitorEventQueue& monitor_queue,
                       app::ConnectionWorker& connection_worker,
                       std::shared_ptr<const profiles::ProfileRegistry> profile_registry,
                       std::vector<profiles::ProfileLoadIssue> profile_issues)
    : connection_worker_(connection_worker) {
    setObjectName("taureonMainWindow");
    setWindowTitle("TAUREON Synth Tool V4");
    resize(1280, 800);

    auto* connection_bar = addToolBar("Connection");
    connection_bar->setObjectName("connectionBar");
    connection_bar->setMovable(false);

    add_caption(*connection_bar, "Backend");
    backend_selector_ = add_selector(*connection_bar,
                                     {"Auto", "Windows MIDI Services", "WinMM"});
    backend_selector_->setObjectName("backendSelector");
    backend_selector_->setAccessibleName("MIDI backend");
    backend_selector_->setToolTip("Choose a backend in either port field to enumerate its ports. No route is opened automatically.");
    add_caption(*connection_bar, "MIDI Input", 6);
    receive_selector_ = new RouteSelector(connection_bar);
    connection_bar->addWidget(receive_selector_);
    receive_selector_->addItem("No input selected");
    receive_selector_->setObjectName("receiveRouteSelector");
    receive_selector_->setAccessibleName("MIDI input route");
    receive_selector_->setEnabled(false);
    add_caption(*connection_bar, "MIDI Output", 6);
    transmit_selector_ = new RouteSelector(connection_bar);
    connection_bar->addWidget(transmit_selector_);
    transmit_selector_->addItem("No output selected");
    transmit_selector_->setObjectName("transmitRouteSelector");
    transmit_selector_->setAccessibleName("MIDI output route");
    transmit_selector_->setEnabled(false);

    connect_button_ = new QPushButton("Connect", connection_bar);
    connect_button_->setObjectName("connectButton");
    connect_button_->setEnabled(false);
    connect_button_->setToolTip("Select an exact RX or TX route before connecting.");
    connection_bar->addWidget(connect_button_);
    auto* panic_button = new QPushButton("Panic", connection_bar);
    panic_button->setObjectName("panicButton");
    panic_button->setEnabled(false);
    panic_button->setToolTip("Panic is unavailable until an explicitly selected TX route is connected.");
    connection_bar->addWidget(panic_button);

    auto* central = new QWidget(this);
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(8, 8, 8, 8);

    navigation_ = new QListWidget(central);
    navigation_->setObjectName("workspaceNavigation");
    for (const auto& name : kWorkspaceNames) navigation_->addItem(name);
    navigation_->setMinimumWidth(220);

    auto* content = new QFrame(central);
    auto* content_layout = new QVBoxLayout(content);
    workspace_heading_ = new QLabel(content);
    workspace_heading_->setObjectName("workspaceHeading");
    workspace_stack_ = new QStackedWidget(content);
    workspace_stack_->setObjectName("workspaceStack");
    monitor_model_ = new MidiMonitorModel(10'000, this);
    monitor_bridge_ = new MonitorEventBridge(monitor_queue, *monitor_model_, this);
    workspace_stack_->addWidget(make_monitor_page(*monitor_model_, *monitor_bridge_, monitor_pause_button_));
    sysex_transfer_panel_ = new SysExTransferPanel(connection_worker_);
    workspace_stack_->addWidget(sysex_transfer_panel_);
    sysex_manager_panel_ = new SysExManagerPanel(
        profile_registry,
        [this](app::SysExManagerTransferItem item,
               SysExManagerPanel::TransferCompletion completion) {
            return sysex_transfer_panel_->request_load_document(
                std::move(item.document), std::move(item.source_name),
                [this, completion = std::move(completion)](const bool loaded) mutable {
                    if (loaded) navigation_->setCurrentRow(1);
                    if (completion) completion(loaded);
                });
        });
    workspace_stack_->addWidget(sysex_manager_panel_);
    workspace_stack_->addWidget(new LibrarianPanel(workspace_stack_));
    profile_panel_ = new ProfileMatchPanel;
    if (profile_registry) profile_panel_->set_available_profiles(profile_registry->profiles());
    profile_panel_->set_profile_load_issue_count(profile_issues.size());
    profile_panel_->set_select_temporary_action([this](std::string profile_id) {
        sysex_transfer_panel_->request_select_temporary_profile(std::move(profile_id));
    });
    profile_panel_->set_remember_binding_action([this] {
        sysex_transfer_panel_->request_remember_overridden_manual_profile();
    });
    sysex_transfer_panel_->set_snapshot_observer([this](const app::SysExTransferSnapshot& snapshot) {
        profile_panel_->present(snapshot.profile_match);
    });
    workspace_stack_->addWidget(profile_panel_);
    diagnostics_log_ = std::make_shared<app::BoundedLog>();
    diagnostic_export_policy_ = std::make_shared<app::DiagnosticExportPolicy>();
    diagnostics_panel_ = new DiagnosticsPanel(
        connection_worker_, monitor_queue, diagnostic_export_policy_, workspace_stack_);
    diagnostics_panel_->set_profile_load_issues(std::move(profile_issues));
    workspace_stack_->addWidget(diagnostics_panel_);
    const auto settings_location = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    settings_panel_ = new SettingsPanel(
        std::filesystem::path{settings_location.toStdWString()} / "taureon-settings.v1",
        diagnostics_log_, diagnostic_export_policy_, workspace_stack_);
    const auto apply_safe_settings = [this](const app::Settings& settings) {
        monitor_model_->set_history_limit(settings.monitor_history_limit);
        sysex_transfer_panel_->set_default_pacing(settings.sysex_pacing_milliseconds);
    };
    settings_panel_->set_applied_settings_callback(apply_safe_settings);
    apply_safe_settings(settings_panel_->settings());
    monitor_pause_button_->setChecked(settings_panel_->settings().monitor_start_paused);
    auto* settings_scroll = new QScrollArea(workspace_stack_);
    settings_scroll->setObjectName("settingsScrollArea");
    settings_scroll->setFrameShape(QFrame::NoFrame);
    settings_scroll->setWidgetResizable(true);
    settings_scroll->setWidget(settings_panel_);
    workspace_stack_->addWidget(settings_scroll);
    content_layout->addWidget(workspace_heading_);
    content_layout->addWidget(workspace_stack_);

    layout->addWidget(navigation_);
    layout->addWidget(content, 1);
    setCentralWidget(central);

    connect(navigation_, &QListWidget::currentRowChanged, this, [this](const int index) {
        select_workspace(index);
    });
    navigation_->setCurrentRow(0);

    connect(backend_selector_, &QComboBox::currentIndexChanged, this,
            [this](const int index) { begin_backend_selection(index); });
    connect(receive_selector_, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                if (backend_selector_->currentIndex() == 0 && index > 0) backend_selector_->setCurrentIndex(index);
                else set_connection_busy(false);
            });
    connect(transmit_selector_, &QComboBox::currentIndexChanged, this,
            [this](int index) {
                if (backend_selector_->currentIndex() == 0 && index > 0) backend_selector_->setCurrentIndex(index);
                else set_connection_busy(false);
            });
    connect(connect_button_, &QPushButton::clicked, this, [this] { begin_connect_toggle(); });
    connection_poll_timer_ = new QTimer(this);
    connection_poll_timer_->setInterval(25);
    connect(connection_poll_timer_, &QTimer::timeout, this, [this] { poll_connection_result(); });
    connection_poll_timer_->start();

    begin_backend_selection(0);
}

MainWindow::~MainWindow() {
    // The observer targets a sibling workspace. Clear it before QWidget child teardown.
    if (sysex_transfer_panel_) sysex_transfer_panel_->set_snapshot_observer({});
    monitor_bridge_->shutdown();
}

bool MainWindow::has_expected_shell() const noexcept {
    return navigation_ != nullptr && workspace_stack_ != nullptr && workspace_heading_ != nullptr &&
           navigation_->count() == static_cast<int>(kWorkspaceNames.size()) &&
           workspace_stack_->count() == static_cast<int>(kWorkspaceNames.size());
}

void MainWindow::select_workspace(const int index) {
    if (index < 0 || index >= workspace_stack_->count()) return;
    workspace_stack_->setCurrentIndex(index);
    workspace_heading_->setText(kWorkspaceNames.at(static_cast<std::size_t>(index)));
}

void MainWindow::begin_backend_selection(const int index) {
    if (pending_connection_) return;
    connection_error_latched_ = false;
    backend_ready_ = false;
    connected_ = false;
    receive_routes_.clear();
    transmit_routes_.clear();
    {
        const QSignalBlocker block_receive(receive_selector_);
        const QSignalBlocker block_transmit(transmit_selector_);
        receive_selector_->clear();
        transmit_selector_->clear();
        receive_selector_->addItem("No input selected");
        transmit_selector_->addItem("No output selected");
    }
    if (index == 0) {
        const QSignalBlocker block_receive(receive_selector_);
        const QSignalBlocker block_transmit(transmit_selector_);
        for (auto* selector : {receive_selector_, transmit_selector_}) {
            selector->setItemText(0, "Choose MIDI backend…");
            selector->addItems({"Windows MIDI Services", "WinMM"});
            selector->setEnabled(true);
        }
        connect_button_->setEnabled(false);
        statusBar()->showMessage(
            "Choose Windows MIDI Services or WinMM in either port field, then select your ports.");
        return;
    }
    const auto backend = index == 1 ? midi::MidiBackend::windows_midi_services :
                                      midi::MidiBackend::winmm;
    pending_connection_ = connection_worker_.select_backend(backend);
    pending_action_ = PendingConnectionAction::backend;
    set_connection_busy(true);
    statusBar()->showMessage("Enumerating the selected backend on the application worker…");
}

void MainWindow::begin_connect_toggle() {
    if (pending_connection_) return;
    connection_error_latched_ = false;
    if (connected_) {
        pending_connection_ = connection_worker_.disconnect();
        pending_action_ = PendingConnectionAction::disconnect;
        set_connection_busy(true);
        statusBar()->showMessage("Disconnecting on the application worker…");
        return;
    }
    const auto receive_index = receive_selector_->currentIndex() - 1;
    const auto transmit_index = transmit_selector_->currentIndex() - 1;
    std::optional<midi::MidiRouteIdentity> receive;
    std::optional<midi::MidiRouteIdentity> transmit;
    if (receive_index >= 0 && receive_index < static_cast<int>(receive_routes_.size())) {
        receive = receive_routes_.at(static_cast<std::size_t>(receive_index));
    }
    if (transmit_index >= 0 && transmit_index < static_cast<int>(transmit_routes_.size())) {
        transmit = transmit_routes_.at(static_cast<std::size_t>(transmit_index));
    }
    if (!receive && !transmit) {
        statusBar()->showMessage("Select an exact MIDI input or output route before connecting.");
        return;
    }
    pending_connection_ = connection_worker_.connect(std::move(receive), std::move(transmit));
    pending_action_ = PendingConnectionAction::connect;
    set_connection_busy(true);
    statusBar()->showMessage("Connecting exact selected routes on the application worker…");
}

void MainWindow::poll_connection_result() {
    using namespace std::chrono_literals;
    if (refresh_pending_ && refresh_pending_->wait_for(0ms) == std::future_status::ready) {
        auto refresh_result = refresh_pending_->get();
        refresh_pending_.reset();
        // A later user request must not be replaced by an older background snapshot.
        if (!pending_connection_) {
            if (refresh_result) {
                apply_connection_snapshot(refresh_result.value(), false, false);
                set_connection_busy(false);
            } else {
                connection_error_latched_ = true;
                statusBar()->showMessage("MIDI status refresh failed: " +
                                         QString::fromStdString(refresh_result.error().message));
            }
        }
    }
    if (!pending_connection_) {
        if (!refresh_pending_ && backend_ready_ &&
            ++idle_poll_ticks_ >= 10) {
            idle_poll_ticks_ = 0;
            refresh_pending_ = connection_worker_.snapshot();
        }
        return;
    }
    if (pending_connection_->wait_for(0ms) != std::future_status::ready) return;
    auto result = pending_connection_->get();
    const auto action = pending_action_;
    pending_connection_.reset();
    if (!result) {
        connected_ = false;
        // Only a failed backend selection leaves no usable route list. After a failed
        // connect or disconnect the enumerated routes stay selectable so the user can retry.
        if (action == PendingConnectionAction::backend) backend_ready_ = false;
        set_connection_busy(false);
        connection_error_latched_ = true;
        statusBar()->showMessage("MIDI operation failed: " + QString::fromStdString(result.error().message));
        return;
    }
    backend_ready_ = true;
    apply_connection_snapshot(result.value(), action == PendingConnectionAction::backend, true);
    if (action == PendingConnectionAction::backend) {
        statusBar()->showMessage(result.value().endpoints.empty() ?
            "Backend available, but no MIDI routes found. Check device availability or choose WinMM." :
            QStringLiteral("%1: %2 input / %3 output routes — select your ports, then Connect.")
                .arg(backend_selector_->currentText()).arg(receive_routes_.size()).arg(transmit_routes_.size()));
    }
    set_connection_busy(false);
}

void MainWindow::apply_connection_snapshot(const app::ConnectionSnapshot& snapshot,
                                           const bool repopulate, const bool user_action) {
    if (settings_panel_) settings_panel_->set_connection_snapshot(snapshot);
    if (repopulate) {
        receive_routes_.clear();
        transmit_routes_.clear();
        {
            const QSignalBlocker block_receive(receive_selector_);
            const QSignalBlocker block_transmit(transmit_selector_);
            receive_selector_->clear();
            transmit_selector_->clear();
            receive_selector_->addItem("No input selected");
            transmit_selector_->addItem("No output selected");
            for (const auto& endpoint : snapshot.endpoints) {
                if (endpoint.identity.direction == midi::MidiDirection::input) {
                    receive_routes_.push_back(endpoint.identity);
                    receive_selector_->addItem(endpoint_name(endpoint));
                    receive_selector_->setItemData(receive_selector_->count() - 1,
                                                  endpoint_label(endpoint), Qt::ToolTipRole);
                } else {
                    transmit_routes_.push_back(endpoint.identity);
                    transmit_selector_->addItem(endpoint_name(endpoint));
                    transmit_selector_->setItemData(transmit_selector_->count() - 1,
                                                   endpoint_label(endpoint), Qt::ToolTipRole);
                }
            }
        }
    }
    const bool state_changed = !last_connection_state_ ||
        *last_connection_state_ != snapshot.state || last_connection_detail_ != snapshot.detail;
    last_connection_state_ = snapshot.state;
    last_connection_detail_ = snapshot.detail;
    connected_ = snapshot.state == app::ConnectionPresentationState::connected ||
                 snapshot.state == app::ConnectionPresentationState::degraded;
    connect_button_->setText(connected_ ? "Disconnect" : "Connect");
    if ((!user_action && !state_changed) || (!user_action && connection_error_latched_)) return;
    switch (snapshot.state) {
    case app::ConnectionPresentationState::connected:
        statusBar()->showMessage("Connected to the exact selected RX/TX routes.");
        break;
    case app::ConnectionPresentationState::degraded:
        statusBar()->showMessage("Degraded: " + QString::fromStdString(snapshot.detail));
        break;
    case app::ConnectionPresentationState::error:
        statusBar()->showMessage("MIDI error: " + QString::fromStdString(snapshot.detail));
        connection_error_latched_ = true;
        break;
    case app::ConnectionPresentationState::ready:
        statusBar()->showMessage(snapshot.endpoints.empty() ?
            "Backend available, but no MIDI routes found. Check device availability or choose WinMM." :
            "Backend ready — select exact RX/TX routes.");
        break;
    case app::ConnectionPresentationState::disconnected:
        statusBar()->showMessage("Disconnected.");
        break;
    }
}

void MainWindow::set_connection_busy(const bool busy) {
    backend_selector_->setEnabled(!busy);
    const bool backend_ready = !busy && backend_ready_;
    const bool choosing_backend = !busy && backend_selector_->currentIndex() == 0;
    receive_selector_->setEnabled((backend_ready || choosing_backend) && !connected_);
    transmit_selector_->setEnabled((backend_ready || choosing_backend) && !connected_);
    const bool route_selected = receive_selector_->currentIndex() > 0 ||
                                transmit_selector_->currentIndex() > 0;
    connect_button_->setEnabled(!busy && (connected_ || (backend_ready && route_selected)));
}

} // namespace taureon::gui
