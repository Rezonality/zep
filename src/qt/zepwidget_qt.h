#pragma once

#include <QWidget>
#include <QTimer>
#include <memory>

namespace Zep
{

class ZepDisplay_Qt;
class ZepWidget_Qt : public QWidget, public IZepComponent
{
public:
    ZepWidget_Qt(QWidget* pParent);
    virtual ~ZepWidget_Qt();

    void paintEvent(QPaintEvent* pPaint) override;
    void keyPressEvent(QKeyEvent* ev) override;

    ZepDisplay_Qt* GetDisplay() const { return m_spDisplay.get(); }

    // IZepComponent
    virtual ZepEditor& GetEditor() const override { return *m_spEditor; }
    virtual void Notify(std::shared_ptr<ZepMessage> message);

private slots:
    void OnTimer();

private:
    std::unique_ptr<ZepDisplay_Qt> m_spDisplay;
    std::unique_ptr<ZepEditor> m_spEditor;
    QTimer m_refreshTimer;
};

}