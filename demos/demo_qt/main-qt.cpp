#include <QApplication>
#include <QGuiApplication>
#include <QTimer>
#include <qmainwindow.h>
#include "mainwindow.h"
#include "zep/qt/common_qt.h"

int main(int argc, char** argv)
{

    QApplication app(argc, argv);

    // Same calculation as the ImGui demo - so that the window sizes should match
    // if DPI calculations work out the same
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect current = screen->geometry();
    float ratio = current.width() / (float)current.height();
    int startWidth = uint32_t(current.width() * .6666);
    int startHeight = uint32_t(startWidth / ratio);

    MainWindow mainWin;
    mainWin.resize(QSize(startWidth, startHeight));
    mainWin.show();

    return app.exec();
}
