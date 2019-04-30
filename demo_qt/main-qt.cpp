#include <QApplication>
#include <QTimer>
#include <qmainwindow.h>
#include "mainwindow.h"
#include "common-qt.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    MainWindow mainWin;
    mainWin.resize(DPI::ScalePixels(800, 600));
    mainWin.show();

    return app.exec();
}
