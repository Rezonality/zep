#pragma once

#include <QWidget>
#include <QTimer>
#include <memory>

namespace Zep
{

class ZepDisplay_Qt;
class ZepWindow_Qt : public QWidget
{
public:
    ZepWindow_Qt(QWidget* pParent);
    virtual ~ZepWindow_Qt();

    void paintEvent(QPaintEvent* pPaint) override;
    void keyPressEvent(QKeyEvent* ev) override;

    ZepEditor* GetEditor() const { return m_spEditor.get(); }
    ZepDisplay_Qt* GetDisplay() const { return m_spDisplay.get(); }

private slots:
    void OnTimer();

private:
    std::unique_ptr<ZepDisplay_Qt> m_spDisplay;
    std::unique_ptr<ZepEditor> m_spEditor;
    QTimer m_refreshTimer;
};

}