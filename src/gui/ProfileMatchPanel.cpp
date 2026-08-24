#include "gui/ProfileMatchPanel.hpp"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace taureon::gui {

ProfileMatchPanel::ProfileMatchPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    selected_ = new QLabel("Selected profile: none", this);
    evidence_ = new QLabel("Evidence: no match evaluated", this);
    override_ = new QLabel(this);
    override_->setObjectName("overriddenManualProfileEvidence");
    override_->setWordWrap(true);
    override_->hide();
    remember_ = new QPushButton("Remember binding", this);
    remember_->setEnabled(false);
    remember_->setToolTip("Available only when a temporary manual choice was overridden by stronger evidence.");
    connect(remember_, &QPushButton::clicked, this, [this] {
        if (remember_action_) remember_action_();
    });
    layout->addWidget(selected_);
    layout->addWidget(evidence_);
    layout->addWidget(override_);
    layout->addWidget(remember_);
    layout->addStretch();
}

void ProfileMatchPanel::present(const profiles::ProfileMatchResult& result) {
    selected_->setText("Selected profile: " +
                       QString::fromStdString(result.selected_profile_id.value_or("none")));
    evidence_->setText("Evidence entries: " + QString::number(result.evidence.size()));
    const auto overridden = std::find_if(result.evidence.begin(), result.evidence.end(),
                                         [](const auto& item) {
                                             return item.kind == profiles::ProfileEvidenceKind::overridden_manual_selection;
                                         });
    const bool visible = overridden != result.evidence.end();
    override_->setVisible(visible);
    remember_->setEnabled(visible && static_cast<bool>(remember_action_));
    if (visible) {
        override_->setText("Temporary choice '" + QString::fromStdString(overridden->profile_id) +
                           "' was overridden by stronger profile evidence. You may deliberately promote it to a saved binding.");
    } else {
        override_->clear();
    }
}

void ProfileMatchPanel::set_remember_binding_action(std::function<void()> action) {
    remember_action_ = std::move(action);
}

bool ProfileMatchPanel::override_is_visible() const { return override_->isVisible(); }
bool ProfileMatchPanel::remember_binding_is_enabled() const { return remember_->isEnabled(); }
void ProfileMatchPanel::trigger_remember_binding() { remember_->click(); }

} // namespace taureon::gui
