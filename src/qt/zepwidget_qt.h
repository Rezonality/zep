#pragma once

#include <QWidget>
#include <QTimer>
#include <memory>

namespace Zep
{

class ZepDisplay_Qt;
class ZepWidget_Qt : public QWidget
    , public IZepComponent
{
public:
    ZepWidget_Qt(QWidget* pParent);
    virtual ~ZepWidget_Qt();

    void resizeEvent(QResizeEvent* pResize) override;
    void paintEvent(QPaintEvent* pPaint) override;
    void keyPressEvent(QKeyEvent* ev) override;

    // IZepComponent
    virtual ZepEditor& GetEditor() const override
    {
        return *m_spEditor;
    }
    virtual void Notify(std::shared_ptr<ZepMessage> message);

private slots:
    void OnTimer();

private:
    std::unique_ptr<ZepEditor> m_spEditor;
    QTimer m_refreshTimer;
};

} // namespace Zep