#pragma once

#include <QComboBox>
#include <QSpinBox>
#include <QWheelEvent>

namespace taureon::gui {

// Let a containing scroll area handle wheel input until the user deliberately focuses
// an editor. Hovering over a preference must not silently change its value.
class FocusWheelComboBox final : public QComboBox {
public:
    explicit FocusWheelComboBox(QWidget* parent = nullptr) : QComboBox(parent) {}

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (!hasFocus()) {
            event->ignore();
            return;
        }
        QComboBox::wheelEvent(event);
    }
};

class FocusWheelSpinBox final : public QSpinBox {
public:
    explicit FocusWheelSpinBox(QWidget* parent = nullptr) : QSpinBox(parent) {}

protected:
    void wheelEvent(QWheelEvent* event) override {
        if (!hasFocus()) {
            event->ignore();
            return;
        }
        QSpinBox::wheelEvent(event);
    }
};

} // namespace taureon::gui
