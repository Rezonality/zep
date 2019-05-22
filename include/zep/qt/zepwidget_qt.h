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
    ZepWidget_Qt(QWidget* pParent, const ZepPath& zepRoot);
    virtual ~ZepWidget_Qt();

    virtual ZepMouseButton GetMouseButton(QMouseEvent* ev);
    virtual void resizeEvent(QResizeEvent* pResize) override;
    virtual void paintEvent(QPaintEvent* pPaint) override;
    virtual void keyPressEvent(QKeyEvent* ev) override;
    
    virtual void mousePressEvent(QMouseEvent *event) override;
    virtual void mouseReleaseEvent(QMouseEvent *event) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *event) override;
    virtual void mouseMoveEvent(QMouseEvent *event) override;

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