#include "gui/SettingsPanel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <string>
#include <utility>
#include <type_traits>

namespace taureon::gui {
namespace {

QString route_text(const std::optional<midi::PersistedMidiRoute>& route) {
    if (!route) return "No exact route preference captured";
    const auto value = midi::serialize_route(*route);
    return value ? QString::fromStdString(value.value()) : "Invalid route preference";
}

QComboBox* enum_box(QWidget* parent, std::initializer_list<std::pair<QString, QString>> values) {
    auto* box = new QComboBox(parent);
    for (const auto& [label, value] : values) box->addItem(label, value);
    return box;
}

QString path_text(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return QString::fromUtf8(reinterpret_cast<const char*>(text.data()), static_cast<qsizetype>(text.size()));
}

std::filesystem::path path_from_text(const QString& value) {
    const auto bytes = value.toUtf8();
    return std::filesystem::u8path(std::string_view(bytes.constData(), static_cast<std::size_t>(bytes.size())));
}

template <typename Enum>
void select_value(QComboBox& box, const Enum value) {
    const auto index = box.findData(QString::fromLatin1(to_string(value)));
    if (index >= 0) box.setCurrentIndex(index);
}

template <typename Enum>
Enum enum_from(QComboBox& box, Enum fallback) {
    const auto value = box.currentData().toString();
    if constexpr (std::is_same_v<Enum, app::ThemePreference>) {
        if (value == "light") return app::ThemePreference::light;
        if (value == "dark") return app::ThemePreference::dark;
        if (value == "system") return app::ThemePreference::system;
    } else if constexpr (std::is_same_v<Enum, app::BackendPreference>) {
        if (value == "wms") return app::BackendPreference::windows_midi_services;
        if (value == "winmm") return app::BackendPreference::winmm;
        if (value == "automatic") return app::BackendPreference::automatic;
    } else if constexpr (std::is_same_v<Enum, app::ReconnectPolicy>) {
        if (value == "manual_only") return app::ReconnectPolicy::manual_only;
        if (value == "require_confirmation") return app::ReconnectPolicy::require_confirmation;
    } else if constexpr (std::is_same_v<Enum, app::ConfirmationPolicy>) {
        if (value == "confirm_when_sending") return app::ConfirmationPolicy::confirm_when_sending;
        if (value == "always_confirm") return app::ConfirmationPolicy::always_confirm;
    } else if constexpr (std::is_same_v<Enum, app::LogLevel>) {
        if (value == "error") return app::LogLevel::error;
        if (value == "warning") return app::LogLevel::warning;
        if (value == "info") return app::LogLevel::info;
        if (value == "debug") return app::LogLevel::debug;
    }
    return fallback;
}

} // namespace

SettingsPanel::SettingsPanel(std::filesystem::path path, std::shared_ptr<app::BoundedLog> log,
                             QWidget* parent)
    : QWidget(parent), path_(std::move(path)), log_(std::move(log)) {
    setObjectName("settingsPanel");
    const auto loaded = app::SettingsStore::load(path_);
    settings_ = loaded.settings;
    auto* layout = new QVBoxLayout(this);

    auto* general = new QGroupBox("General", this);
    auto* general_form = new QFormLayout(general);
    theme_ = enum_box(general, {{"System compatible", "system"}, {"Light", "light"}, {"Dark", "dark"}});
    theme_->setObjectName("settingsTheme");
    ui_scale_ = new QSpinBox(general);
    ui_scale_->setRange(50, 300);
    ui_scale_->setSuffix("%");
    ui_scale_->setObjectName("settingsUiScale");
    standard_path_ = new QLineEdit(general);
    standard_path_->setObjectName("settingsStandardPath");
    restore_session_ = new QCheckBox("Restore safe session preferences on next launch", general);
    restore_session_->setObjectName("settingsRestoreSession");
    general_form->addRow("Appearance", theme_);
    general_form->addRow("UI scale preference", ui_scale_);
    general_form->addRow("Standard path", standard_path_);
    general_form->addRow({}, restore_session_);

    auto* midi = new QGroupBox("MIDI preferences", this);
    auto* midi_form = new QFormLayout(midi);
    backend_ = enum_box(midi, {{"Auto (no implicit route restore)", "automatic"}, {"Windows MIDI Services", "wms"}, {"WinMM", "winmm"}});
    backend_->setObjectName("settingsPreferredBackend");
    reconnect_ = enum_box(midi, {{"Require confirmation", "require_confirmation"}, {"Manual only", "manual_only"}});
    reconnect_->setObjectName("settingsReconnectPolicy");
    monitor_paused_ = new QCheckBox("Start monitor presentation paused", midi);
    monitor_paused_->setObjectName("settingsMonitorPaused");
    monitor_history_ = new QSpinBox(midi);
    monitor_history_->setRange(1, 1'000'000);
    monitor_history_->setObjectName("settingsMonitorHistory");
    receive_route_ = new QLabel(midi);
    receive_route_->setObjectName("settingsPreferredReceiveRoute");
    transmit_route_ = new QLabel(midi);
    transmit_route_->setObjectName("settingsPreferredTransmitRoute");
    receive_route_->setWordWrap(true);
    transmit_route_->setWordWrap(true);
    capture_routes_ = new QPushButton("Capture current exact routes as preferences", midi);
    capture_routes_->setObjectName("settingsCaptureExactRoutes");
    capture_routes_->setToolTip("Stores only the current exact Stage-2 route identities. It never opens, closes, or changes a route.");
    midi_form->addRow("Preferred backend", backend_);
    midi_form->addRow("Reconnect policy", reconnect_);
    midi_form->addRow("Monitor history limit", monitor_history_);
    midi_form->addRow({}, monitor_paused_);
    midi_form->addRow("Preferred RX route", receive_route_);
    midi_form->addRow("Preferred TX route", transmit_route_);
    midi_form->addRow({}, capture_routes_);

    auto* sysex = new QGroupBox("SysEx safety defaults", this);
    auto* sysex_form = new QFormLayout(sysex);
    pacing_ = new QSpinBox(sysex);
    pacing_->setRange(0, 60'000);
    pacing_->setSuffix(" ms");
    pacing_->setObjectName("settingsSysExPacing");
    confirmation_ = enum_box(sysex, {{"Always confirm", "always_confirm"}, {"Confirm before sending", "confirm_when_sending"}});
    confirmation_->setObjectName("settingsConfirmationPolicy");
    stop_on_loss_ = new QCheckBox("Stop capture on reported data loss", sysex);
    stop_on_loss_->setObjectName("settingsStopCaptureOnLoss");
    sysex_form->addRow("Generic pacing", pacing_);
    sysex_form->addRow("Confirmation policy", confirmation_);
    sysex_form->addRow({}, stop_on_loss_);

    auto* diagnostics = new QGroupBox("Logging and diagnostics", this);
    auto* diagnostics_form = new QFormLayout(diagnostics);
    log_level_ = enum_box(diagnostics, {{"Error", "error"}, {"Warning", "warning"}, {"Info", "info"}, {"Debug", "debug"}});
    log_level_->setObjectName("settingsLogLevel");
    log_destination_ = new QLineEdit(diagnostics);
    log_destination_->setObjectName("settingsLogDestination");
    log_rotation_ = new QSpinBox(diagnostics);
    log_rotation_->setRange(1, 1'000'000);
    log_rotation_->setObjectName("settingsLogRotation");
    include_routes_ = new QCheckBox("Include exact route identity metadata in diagnostic bundles", diagnostics);
    include_routes_->setObjectName("settingsBundleRouteIdentity");
    diagnostics_form->addRow("Log level", log_level_);
    diagnostics_form->addRow("Log destination", log_destination_);
    diagnostics_form->addRow("Bounded log entries", log_rotation_);
    diagnostics_form->addRow({}, include_routes_);

    save_button_ = new QPushButton("Save Settings", this);
    save_button_->setObjectName("settingsSave");
    status_ = new QLabel(this);
    status_->setObjectName("settingsStatus");
    status_->setWordWrap(true);
    layout->addWidget(general);
    layout->addWidget(midi);
    layout->addWidget(sysex);
    layout->addWidget(diagnostics);
    layout->addWidget(save_button_);
    layout->addWidget(status_);
    layout->addStretch();

    connect(capture_routes_, &QPushButton::clicked, this, [this] {
        if (connection_.receive_route) {
            settings_.preferred_receive_route = {midi::current_route_schema_version, *connection_.receive_route};
        }
        if (connection_.transmit_route) {
            settings_.preferred_transmit_route = {midi::current_route_schema_version, *connection_.transmit_route};
        }
        update_route_labels();
        set_status("Captured only the currently observed exact route identities; no connection changed.");
    });
    connect(save_button_, &QPushButton::clicked, this, [this] {
        const auto result = save();
        set_status(result ? "Settings saved atomically. Preferences are not applied to the active connection."
                          : "Settings save failed: " + result.error().message);
    });
    populate_controls();
    set_status(loaded.detail + ". Settings never auto-select an active route.");
}

void SettingsPanel::set_connection_snapshot(const app::ConnectionSnapshot& snapshot) {
    connection_ = snapshot;
    capture_routes_->setEnabled(static_cast<bool>(connection_.receive_route || connection_.transmit_route));
}

midi::Result<void> SettingsPanel::save() {
    read_controls();
    const auto result = app::SettingsStore::save(path_, settings_);
    if (result && log_) {
        log_->configure(settings_.log_level, settings_.log_rotation_entries);
        log_->append(app::LogLevel::info, "Settings saved without applying a route or connection.");
    }
    return result;
}

const app::Settings& SettingsPanel::settings() const noexcept { return settings_; }

bool SettingsPanel::has_required_controls() const noexcept {
    return status_ && receive_route_ && transmit_route_ && theme_ && ui_scale_ && standard_path_ &&
           restore_session_ && backend_ && reconnect_ && monitor_paused_ && monitor_history_ && pacing_ &&
           confirmation_ && stop_on_loss_ && log_level_ && log_destination_ && log_rotation_ &&
           include_routes_ && capture_routes_ && save_button_;
}

void SettingsPanel::populate_controls() {
    select_value(*theme_, settings_.theme);
    ui_scale_->setValue(static_cast<int>(settings_.ui_scale_percent));
    standard_path_->setText(path_text(settings_.standard_path));
    restore_session_->setChecked(settings_.restore_last_session);
    select_value(*backend_, settings_.preferred_backend);
    select_value(*reconnect_, settings_.reconnect_policy);
    monitor_paused_->setChecked(settings_.monitor_start_paused);
    monitor_history_->setValue(static_cast<int>(settings_.monitor_history_limit));
    pacing_->setValue(static_cast<int>(settings_.sysex_pacing_milliseconds));
    select_value(*confirmation_, settings_.confirmation_policy);
    stop_on_loss_->setChecked(settings_.stop_capture_on_data_loss);
    select_value(*log_level_, settings_.log_level);
    log_destination_->setText(path_text(settings_.log_destination));
    log_rotation_->setValue(static_cast<int>(settings_.log_rotation_entries));
    include_routes_->setChecked(settings_.include_route_identity_in_bundle);
    update_route_labels();
    capture_routes_->setEnabled(false);
}

void SettingsPanel::read_controls() {
    settings_.theme = enum_from(*theme_, app::ThemePreference::system);
    settings_.ui_scale_percent = static_cast<std::uint32_t>(ui_scale_->value());
    settings_.standard_path = path_from_text(standard_path_->text());
    settings_.restore_last_session = restore_session_->isChecked();
    settings_.preferred_backend = enum_from(*backend_, app::BackendPreference::automatic);
    settings_.reconnect_policy = enum_from(*reconnect_, app::ReconnectPolicy::require_confirmation);
    settings_.monitor_start_paused = monitor_paused_->isChecked();
    settings_.monitor_history_limit = static_cast<std::size_t>(monitor_history_->value());
    settings_.sysex_pacing_milliseconds = static_cast<std::uint32_t>(pacing_->value());
    settings_.confirmation_policy = enum_from(*confirmation_, app::ConfirmationPolicy::always_confirm);
    settings_.stop_capture_on_data_loss = stop_on_loss_->isChecked();
    settings_.log_level = enum_from(*log_level_, app::LogLevel::info);
    settings_.log_destination = path_from_text(log_destination_->text());
    settings_.log_rotation_entries = static_cast<std::size_t>(log_rotation_->value());
    settings_.include_route_identity_in_bundle = include_routes_->isChecked();
}

void SettingsPanel::update_route_labels() {
    receive_route_->setText(route_text(settings_.preferred_receive_route));
    transmit_route_->setText(route_text(settings_.preferred_transmit_route));
}

void SettingsPanel::set_status(std::string text) {
    status_->setText(QString::fromStdString(std::move(text)));
}

} // namespace taureon::gui
