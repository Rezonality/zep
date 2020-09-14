#pragma once

#include <QMainWindow>
#include <memory>

#include <orca/mode_orca.h>
#include <repl/mode_repl.h>

QT_FORWARD_DECLARE_CLASS(QDockWidget)
QT_FORWARD_DECLARE_CLASS(QMenu)
QT_FORWARD_DECLARE_CLASS(QToolbar)

class MainWindow : public QMainWindow, public Zep::IZepReplProvider
{
public:
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow();

    virtual std::string ReplParse(Zep::ZepBuffer& buffer, const Zep::GlyphIterator& cursorOffset, Zep::ReplParseType type) override;
    virtual std::string ReplParse(const std::string& text) override;
    virtual bool ReplIsFormComplete(const std::string& str, int& indent) override;

private:
};
