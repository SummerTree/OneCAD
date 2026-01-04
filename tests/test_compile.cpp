#include "ui/viewport/Viewport.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    [[maybe_unused]] onecad::ui::Viewport viewport;
    return 0;
}
