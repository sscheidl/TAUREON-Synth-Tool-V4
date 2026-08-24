#include "gui/MainWindow.hpp"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStringList>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <cstddef>

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

QComboBox* add_disabled_selector(QToolBar& bar, const QStringList& values,
                                 const QString& explanation) {
    auto* selector = new QComboBox(&bar);
    selector->addItems(values);
    selector->setEnabled(false);
    selector->setToolTip(explanation);
    bar.addWidget(selector);
    return selector;
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

} // namespace

MainWindow::MainWindow() {
    setObjectName("taureonMainWindow");
    setWindowTitle("TAUREON Synth Tool V4");
    resize(1280, 800);

    auto* connection_bar = addToolBar("Connection");
    connection_bar->setObjectName("connectionBar");
    connection_bar->setMovable(false);

    const QString unavailable = "Connection selection is not active until the connection-controller slice is complete.";
    add_caption(*connection_bar, "Backend");
    add_disabled_selector(*connection_bar,
                          {"Auto", "Windows MIDI Services", "WinMM"}, unavailable);
    add_caption(*connection_bar, "MIDI Input");
    add_disabled_selector(*connection_bar, {"No input selected"}, unavailable);
    add_caption(*connection_bar, "MIDI Output");
    add_disabled_selector(*connection_bar, {"No output selected"}, unavailable);

    auto* connect_button = new QPushButton("Connect", connection_bar);
    connect_button->setEnabled(false);
    connect_button->setToolTip(unavailable);
    connection_bar->addWidget(connect_button);
    auto* panic_button = new QPushButton("Panic", connection_bar);
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
    for (const auto& name : kWorkspaceNames) workspace_stack_->addWidget(make_workspace_page(name));
    content_layout->addWidget(workspace_heading_);
    content_layout->addWidget(workspace_stack_);

    layout->addWidget(navigation_);
    layout->addWidget(content, 1);
    setCentralWidget(central);

    connect(navigation_, &QListWidget::currentRowChanged, this, [this](const int index) {
        select_workspace(index);
    });
    navigation_->setCurrentRow(0);

    statusBar()->showMessage("Disconnected — no MIDI route is selected.");
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

} // namespace taureon::gui
