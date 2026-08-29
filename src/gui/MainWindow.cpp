#include "gui/MainWindow.hpp"
#include "gui/MidiMonitorModel.hpp"
#include "gui/MidiMonitorFilterModel.hpp"
#include "gui/MonitorEventBridge.hpp"
#include "gui/ProfileMatchPanel.hpp"
#include "gui/SysExTransferPanel.hpp"
#include "gui/SysExManagerPanel.hpp"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QToolBar>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

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

QLabel* add_caption(QToolBar& bar, const QString& caption) {
    auto* label = new QLabel(caption, &bar);
    label->setObjectName("connectionCaption");
    bar.addWidget(label);
    return label;
}

QComboBox* add_selector(QToolBar& bar, const QStringList& values) {
    auto* selector = new QComboBox(&bar);
    selector->addItems(values);
    bar.addWidget(selector);
    return selector;
}

QString endpoint_label(const midi::MidiEndpointDescriptor& endpoint) {
    return std::visit(
        [&endpoint](const auto& identity) -> QString {
            using Identity = std::decay_t<decltype(identity)>;
            if constexpr (std::is_same_v<Identity, midi::WmsRouteIdentity>) {
                return QStringLiteral("%1 — %2 · group %3")
                    .arg(QString::fromStdString(endpoint.display_name),
                         QString::fromStdString(identity.endpoint_device_id))
                    .arg(identity.group + 1);
            } else {
                return QStringLiteral("%1 — manufacturer %2 · product %3 · driver %4")
                    .arg(QString::fromStdString(endpoint.display_name))
                    .arg(identity.manufacturer_id)
                    .arg(identity.product_id)
                    .arg(identity.driver_version);
            }
        }, endpoint.identity.native);
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

QWidget* make_monitor_page(MidiMonitorModel& model, MonitorEventBridge& bridge) {
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    auto* controls = new QHBoxLayout;
    auto* direction = new QComboBox(page);
    direction->addItems({"All directions", "RX", "TX"});
    direction->setAccessibleName("Monitor direction filter");
    auto* type_filter = new QLineEdit(page);
    type_filter->setPlaceholderText("Filter event type");
    type_filter->setAccessibleName("Monitor event type filter");
    auto* pause = new QPushButton("Pause presentation", page);
    pause->setCheckable(true);
    pause->setToolTip("Presentation events are counted and discarded while paused; transport capture continues.");
    auto* clear = new QPushButton("Clear", page);
    auto* accounting = new QLabel("Presentation running", page);
    accounting->setObjectName("monitorPresentationAccounting");
    controls->addWidget(direction);
    controls->addWidget(type_filter, 1);
    controls->addWidget(pause);
    controls->addWidget(clear);
    controls->addWidget(accounting);
    layout->addLayout(controls);

    auto* proxy = new MidiMonitorFilterModel(page);
    proxy->setSourceModel(&model);
    auto* table = new QTableView(page);
    table->setObjectName("midiMonitorTable");
    table->setModel(proxy);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSortingEnabled(false);
    layout->addWidget(table);

    QObject::connect(direction, &QComboBox::currentTextChanged, page,
                     [proxy](const QString& value) {
                         proxy->set_direction(value == "All directions" ? QString{} : value);
                     });
    QObject::connect(type_filter, &QLineEdit::textChanged, page,
                     [proxy](const QString& value) { proxy->set_type_filter(value); });
    QObject::connect(pause, &QPushButton::toggled, page, [&bridge, pause](const bool paused) {
        bridge.set_paused(paused);
        pause->setText(paused ? "Resume presentation" : "Pause presentation");
    });
    QObject::connect(clear, &QPushButton::clicked, page, [&model] { model.clear(); });
    auto* accounting_timer = new QTimer(page);
    accounting_timer->setInterval(250);
    QObject::connect(accounting_timer, &QTimer::timeout, page, [&bridge, accounting] {
        const auto stats = bridge.presentation_stats();
        accounting->setText(
            QStringLiteral("%1 · displayed %2 · paused-discarded %3")
                .arg(stats.paused ? "Paused" : "Running")
                .arg(stats.displayed)
                .arg(stats.discarded_while_paused));
    });
    accounting_timer->start();
    return page;
}

} // namespace

MainWindow::MainWindow(app::MonitorEventQueue& monitor_queue,
                       app::ConnectionWorker& connection_worker,
                       std::shared_ptr<const profiles::ProfileRegistry> profile_registry)
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
    backend_selector_->setToolTip("Auto restores only an exactly resolvable saved route; none is saved yet.");
    add_caption(*connection_bar, "MIDI Input");
    receive_selector_ = add_selector(*connection_bar, {"No input selected"});
    receive_selector_->setObjectName("receiveRouteSelector");
    receive_selector_->setAccessibleName("MIDI input route");
    receive_selector_->setEnabled(false);
    add_caption(*connection_bar, "MIDI Output");
    transmit_selector_ = add_selector(*connection_bar, {"No output selected"});
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
    layout->setContentsMargins(12, 12, 12, 12);

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
    workspace_stack_->addWidget(make_monitor_page(*monitor_model_, *monitor_bridge_));
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
    workspace_stack_->addWidget(make_workspace_page(kWorkspaceNames.at(3)));
    profile_panel_ = new ProfileMatchPanel;
    if (profile_registry) profile_panel_->set_available_profiles(profile_registry->profiles());
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
    for (std::size_t index = 5; index < kWorkspaceNames.size(); ++index) {
        workspace_stack_->addWidget(make_workspace_page(kWorkspaceNames.at(index)));
    }
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
            [this] { set_connection_busy(false); });
    connect(transmit_selector_, &QComboBox::currentIndexChanged, this,
            [this] { set_connection_busy(false); });
    connect(connect_button_, &QPushButton::clicked, this, [this] { begin_connect_toggle(); });
    connection_poll_timer_ = new QTimer(this);
    connection_poll_timer_->setInterval(25);
    connect(connection_poll_timer_, &QTimer::timeout, this, [this] { poll_connection_result(); });
    connection_poll_timer_->start();

    statusBar()->showMessage(
        "Disconnected — Auto has no exactly resolvable saved route; choose a backend and routes.");
}

MainWindow::~MainWindow() { monitor_bridge_->shutdown(); }

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
    connected_ = false;
    receive_routes_.clear();
    transmit_routes_.clear();
    receive_selector_->clear();
    transmit_selector_->clear();
    receive_selector_->addItem("No input selected");
    transmit_selector_->addItem("No output selected");
    if (index == 0) {
        receive_selector_->setEnabled(false);
        transmit_selector_->setEnabled(false);
        connect_button_->setEnabled(false);
        statusBar()->showMessage(
            "Auto requires an exactly resolvable saved backend and routes; deliberate selection is required.");
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
    if (!pending_connection_) {
        if (backend_selector_->currentIndex() > 0 && ++idle_poll_ticks_ >= 10) {
            idle_poll_ticks_ = 0;
            pending_connection_ = connection_worker_.snapshot();
            pending_action_ = PendingConnectionAction::snapshot;
        }
        return;
    }
    if (pending_connection_->wait_for(0ms) != std::future_status::ready) return;
    auto result = pending_connection_->get();
    const auto action = pending_action_;
    pending_connection_.reset();
    if (!result) {
        connected_ = false;
        set_connection_busy(false);
        statusBar()->showMessage("MIDI operation failed: " + QString::fromStdString(result.error().message));
        return;
    }
    apply_connection_snapshot(result.value(), action == PendingConnectionAction::backend);
    set_connection_busy(false);
}

void MainWindow::apply_connection_snapshot(const app::ConnectionSnapshot& snapshot,
                                           const bool repopulate) {
    if (repopulate) {
        receive_routes_.clear();
        transmit_routes_.clear();
        receive_selector_->clear();
        transmit_selector_->clear();
        receive_selector_->addItem("No input selected");
        transmit_selector_->addItem("No output selected");
        for (const auto& endpoint : snapshot.endpoints) {
            if (endpoint.identity.direction == midi::MidiDirection::input) {
                receive_routes_.push_back(endpoint.identity);
                receive_selector_->addItem(endpoint_label(endpoint));
            } else {
                transmit_routes_.push_back(endpoint.identity);
                transmit_selector_->addItem(endpoint_label(endpoint));
            }
        }
    }
    connected_ = snapshot.state == app::ConnectionPresentationState::connected ||
                 snapshot.state == app::ConnectionPresentationState::degraded;
    connect_button_->setText(connected_ ? "Disconnect" : "Connect");
    switch (snapshot.state) {
    case app::ConnectionPresentationState::connected:
        statusBar()->showMessage("Connected to the exact selected RX/TX routes.");
        break;
    case app::ConnectionPresentationState::degraded:
        statusBar()->showMessage("Degraded: " + QString::fromStdString(snapshot.detail));
        break;
    case app::ConnectionPresentationState::error:
        statusBar()->showMessage("MIDI error: " + QString::fromStdString(snapshot.detail));
        break;
    case app::ConnectionPresentationState::ready:
        statusBar()->showMessage("Backend ready — select exact RX/TX routes.");
        break;
    case app::ConnectionPresentationState::disconnected:
        statusBar()->showMessage("Disconnected.");
        break;
    }
}

void MainWindow::set_connection_busy(const bool busy) {
    backend_selector_->setEnabled(!busy);
    const bool backend_ready = !busy && backend_selector_->currentIndex() > 0;
    receive_selector_->setEnabled(backend_ready && !connected_);
    transmit_selector_->setEnabled(backend_ready && !connected_);
    const bool route_selected = receive_selector_->currentIndex() > 0 ||
                                transmit_selector_->currentIndex() > 0;
    connect_button_->setEnabled(!busy && (connected_ || (backend_ready && route_selected)));
}

} // namespace taureon::gui
