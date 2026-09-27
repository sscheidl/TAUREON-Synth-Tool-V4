#include "gui/ProfileMatchPanel.hpp"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <utility>

namespace taureon::gui {
namespace {

QString optional_text(const std::optional<std::string>& value) {
    return value ? QString::fromStdString(*value) : QStringLiteral("not declared");
}

QString profile_details(const profiles::DeviceProfile& profile,
                        const profiles::ProfileMatchResult& match) {
    QStringList lines{
        "Name: " + QString::fromStdString(profile.display_name),
        "ID: " + QString::fromStdString(profile.profile_id),
        "Profile version: " + QString::fromStdString(profile.profile_version),
        "Schema version: " + QString::number(profile.schema_version),
        "Manufacturer: " + optional_text(profile.manufacturer),
        "Model: " + optional_text(profile.model),
        "Variant: " + optional_text(profile.variant),
        "Firmware scope: not declared in this profile schema",
        "Type: " + QString(profile.generic ? "Generic fallback" : "Device profile"),
        "Protocol module: " + optional_text(profile.protocol_module_id),
        {}, "Declared capabilities:"};
    constexpr auto claims = std::array{
        std::pair{"Detect", profiles::SupportLevel::detect},
        std::pair{"Read", profiles::SupportLevel::read},
        std::pair{"Inspect", profiles::SupportLevel::inspect},
        std::pair{"Extract", profiles::SupportLevel::extract},
        std::pair{"Modify", profiles::SupportLevel::modify},
        std::pair{"Serialize", profiles::SupportLevel::serialize},
        std::pair{"Transfer", profiles::SupportLevel::transfer},
        std::pair{"Validated restore", profiles::SupportLevel::validated_restore},
    };
    for (const auto& [name, level] : claims) {
        lines.push_back(QStringLiteral("  %1: %2").arg(name, profile.support.supports(level) ? "yes" : "no"));
    }
    lines.push_back({});
    lines.push_back("Recognition:");
    lines.push_back(QStringLiteral("  Native identity: %1").arg(profile.recognition.native_identity ? "declared" : "none"));
    lines.push_back(QStringLiteral("  Universal identity: %1").arg(profile.recognition.universal_identity ? "declared" : "none"));
    for (const auto& fingerprint : profile.recognition.sysex_fingerprints) {
        lines.push_back(QStringLiteral("  SysEx fingerprint %1: offset %2, %3 bytes")
                            .arg(QString::fromStdString(fingerprint.id))
                            .arg(fingerprint.offset)
                            .arg(fingerprint.bytes.size()));
    }
    lines.push_back({});
    lines.push_back("Current match evidence:");
    bool has_evidence = false;
    for (const auto& evidence : match.evidence) {
        if (evidence.profile_id != profile.profile_id) continue;
        lines.push_back("  " + QString::fromStdString(evidence.detail));
        has_evidence = true;
    }
    if (!has_evidence) lines.push_back("  None observed for this profile");
    lines.push_back({});
    lines.push_back("Warnings:");
    if (profile.warnings.empty()) lines.push_back("  None declared");
    for (const auto& warning : profile.warnings) lines.push_back("  " + QString::fromStdString(warning));
    lines.push_back({});
    lines.push_back("Provenance:");
    lines.push_back("  Source: " + QString::fromStdString(profile.provenance.source));
    lines.push_back("  Owner: " + QString::fromStdString(profile.provenance.owner));
    lines.push_back("  License: " + QString::fromStdString(profile.provenance.license));
    lines.push_back("  Redistribution: " + QString::fromStdString(profile.provenance.redistribution));
    return lines.join('\n');
}

} // namespace

ProfileMatchPanel::ProfileMatchPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("profileMatchPanel");
    auto* layout = new QVBoxLayout(this);
    selected_ = new QLabel("Selected profile: none", this);
    selected_->setObjectName("profileSelectedProfile");
    evidence_ = new QLabel("Evidence: no match evaluated", this);
    evidence_->setObjectName("profileEvidence");
    override_ = new QLabel(this);
    override_->setObjectName("overriddenManualProfileEvidence");
    override_->setWordWrap(true);
    override_->hide();
    load_issues_ = new QLabel(this);
    load_issues_->setWordWrap(true);
    load_issues_->hide();

    temporary_selector_ = new QComboBox(this);
    temporary_selector_->setObjectName("profileTemporarySelector");
    temporary_selector_->setAccessibleName("Temporary MIDI profile");
    temporary_selector_->addItem("Choose a temporary profile…", QString{});
    use_temporary_ = new QPushButton("Use temporarily", this);
    use_temporary_->setObjectName("profileUseTemporarily");
    use_temporary_->setToolTip(
        "Uses the selected profile only for this application session; it does not bind a MIDI route.");
    use_temporary_->setEnabled(false);

    remember_ = new QPushButton("Remember binding", this);
    remember_->setObjectName("profileRememberBinding");
    remember_->setEnabled(false);
    remember_->setToolTip(
        "Available only when a temporary manual choice was overridden by stronger evidence.");
    details_ = new QPushButton("Profile details…", this);
    details_->setEnabled(false);

    connect(temporary_selector_, &QComboBox::currentIndexChanged, this,
            [this] { update_temporary_selection_enabled(); });
    connect(use_temporary_, &QPushButton::clicked, this, [this] {
        if (!select_temporary_action_) return;
        const auto profile_id = temporary_selector_->currentData().toString().toStdString();
        if (profile_id.empty()) return;
        select_temporary_action_(profile_id);
    });
    connect(remember_, &QPushButton::clicked, this, [this] {
        if (remember_action_) remember_action_();
    });
    connect(details_, &QPushButton::clicked, this, [this] { show_profile_details(); });

    layout->addWidget(selected_);
    layout->addWidget(evidence_);
    layout->addWidget(override_);
    layout->addWidget(load_issues_);
    layout->addWidget(temporary_selector_);
    layout->addWidget(use_temporary_);
    layout->addWidget(remember_);
    layout->addWidget(details_);
    layout->addStretch();
}

void ProfileMatchPanel::set_available_profiles(
    const std::vector<profiles::DeviceProfile>& profiles) {
    profiles_ = profiles;
    temporary_selector_->clear();
    temporary_selector_->addItem("Choose a temporary profile…", QString{});
    for (const auto& profile : profiles) {
        if (profile.generic) continue;
        temporary_selector_->addItem(QString::fromStdString(profile.display_name),
                                     QString::fromStdString(profile.profile_id));
    }
    update_temporary_selection_enabled();
    details_->setEnabled(!profiles_.empty());
}

void ProfileMatchPanel::set_profile_load_issue_count(const std::size_t count) {
    load_issues_->setVisible(count > 0);
    load_issues_->setText(QStringLiteral("%1 profile load issue(s). See Diagnostics for details.").arg(count));
}

void ProfileMatchPanel::present(const profiles::ProfileMatchResult& result) {
    current_match_ = result;
    selected_->setText("Selected profile: " +
                       QString::fromStdString(result.selected_profile_id.value_or("none")));
    evidence_->setText("Evidence: " + QString::fromStdString(result.message) +
                       " (" + QString::number(result.evidence.size()) + " entries)");
    const auto overridden = std::find_if(
        result.evidence.begin(), result.evidence.end(), [](const auto& item) {
            return item.kind == profiles::ProfileEvidenceKind::overridden_manual_selection;
        });
    const bool visible = overridden != result.evidence.end();
    override_->setVisible(visible);
    remember_->setEnabled(visible && static_cast<bool>(remember_action_));
    if (visible) {
        override_->setText(
            "Temporary choice '" + QString::fromStdString(overridden->profile_id) +
            "' was overridden by stronger profile evidence. You may deliberately promote it to a saved binding.");
    } else {
        override_->clear();
    }
}

void ProfileMatchPanel::show_profile_details() {
    if (profiles_.empty()) return;
    QDialog dialog(this);
    dialog.setWindowTitle("Device profile details");
    dialog.resize(620, 480);
    auto* layout = new QVBoxLayout(&dialog);
    auto* selector = new QComboBox(&dialog);
    for (const auto& profile : profiles_) {
        selector->addItem(QString::fromStdString(profile.display_name),
                          QString::fromStdString(profile.profile_id));
    }
    auto* text = new QPlainTextEdit(&dialog);
    text->setReadOnly(true);
    const auto update = [this, selector, text] {
        const auto id = selector->currentData().toString().toStdString();
        const auto found = std::find_if(profiles_.begin(), profiles_.end(), [&id](const auto& profile) {
            return profile.profile_id == id;
        });
        text->setPlainText(found == profiles_.end() ? QString{} : profile_details(*found, current_match_));
    };
    connect(selector, &QComboBox::currentIndexChanged, &dialog, update);
    if (current_match_.selected_profile_id) {
        const auto index = selector->findData(QString::fromStdString(*current_match_.selected_profile_id));
        if (index >= 0) selector->setCurrentIndex(index);
    }
    update();
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(selector);
    layout->addWidget(text, 1);
    layout->addWidget(buttons);
    dialog.exec();
}

void ProfileMatchPanel::set_select_temporary_action(std::function<void(std::string)> action) {
    select_temporary_action_ = std::move(action);
    update_temporary_selection_enabled();
}

void ProfileMatchPanel::set_remember_binding_action(std::function<void()> action) {
    remember_action_ = std::move(action);
    remember_->setEnabled(!override_->isHidden() && static_cast<bool>(remember_action_));
}

bool ProfileMatchPanel::override_is_visible() const { return !override_->isHidden(); }
bool ProfileMatchPanel::remember_binding_is_enabled() const { return remember_->isEnabled(); }
void ProfileMatchPanel::trigger_remember_binding() { remember_->click(); }

void ProfileMatchPanel::update_temporary_selection_enabled() {
    use_temporary_->setEnabled(static_cast<bool>(select_temporary_action_) &&
                               !temporary_selector_->currentData().toString().isEmpty());
}

} // namespace taureon::gui
