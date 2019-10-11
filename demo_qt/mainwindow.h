#pragma once

#include <QMainWindow>
#include <memory>

QT_FORWARD_DECLARE_CLASS(QDockWidget)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QToolbar)

class MainWindow : public QMainWindow
{
public:
    Q_OBJECT

public:
    MainWindow();

private:
};
