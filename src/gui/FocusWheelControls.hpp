#pragma once

#include <QComboBox>
#include <QSpinBox>
#include <QWheelEvent>

namespace taureon::gui {

// Let a containing scroll area handle wheel input. Settings can disable wheel edits
// even after focus; other controls may allow deliberate focused wheel changes.
class FocusWheelComboBox final : public QComboBox {
public:
    explicit FocusWheelComboBox(QWidget* parent = nullptr, bool allow_focused_wheel = true)
        : QComboBox(parent), allow_focused_wheel_(allow_focused_wheel) {}

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (!allow_focused_wheel_ || !hasFocus()) {
            event->ignore();
            return;
        }
        QComboBox::wheelEvent(event);
    }

private:
    bool allow_focused_wheel_;
};

class FocusWheelSpinBox final : public QSpinBox {
public:
    explicit FocusWheelSpinBox(QWidget* parent = nullptr, bool allow_focused_wheel = true)
        : QSpinBox(parent), allow_focused_wheel_(allow_focused_wheel) {}

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (!allow_focused_wheel_ || !hasFocus()) {
            event->ignore();
            return;
        }
        QSpinBox::wheelEvent(event);
    }

private:
    bool allow_focused_wheel_;
};

} // namespace taureon::gui
