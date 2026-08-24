#include "gui/MainWindow.hpp"

#include <QApplication>

#include <algorithm>
#include <string_view>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    taureon::gui::MainWindow window;
    const bool smoke_test = std::any_of(argv + 1, argv + argc, [](const char* argument) {
        return std::string_view(argument) == "--smoke-test";
    });
    if (smoke_test) {
        return window.has_expected_shell() ? 0 : 1;
    }

    window.show();
    return app.exec();
}
