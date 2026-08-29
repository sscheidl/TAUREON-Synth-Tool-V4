#include "gui/ProfileMatchPanel.hpp"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace taureon::gui {

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

    layout->addWidget(selected_);
    layout->addWidget(evidence_);
    layout->addWidget(override_);
    layout->addWidget(temporary_selector_);
    layout->addWidget(use_temporary_);
    layout->addWidget(remember_);
    layout->addStretch();
}

void ProfileMatchPanel::set_available_profiles(
    const std::vector<profiles::DeviceProfile>& profiles) {
    temporary_selector_->clear();
    temporary_selector_->addItem("Choose a temporary profile…", QString{});
    for (const auto& profile : profiles) {
        if (profile.generic) continue;
        temporary_selector_->addItem(QString::fromStdString(profile.display_name),
                                     QString::fromStdString(profile.profile_id));
    }
    update_temporary_selection_enabled();
}

void ProfileMatchPanel::present(const profiles::ProfileMatchResult& result) {
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
