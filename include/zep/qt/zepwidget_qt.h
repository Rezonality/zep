#pragma once

#include <QTimer>
#include <QWidget>
#include <memory>

#include <QClipboard>
#include <QDebug>
#include <QDesktopWidget>
#include <QKeyEvent>

#include "zep/editor.h"
#include "zep/mode.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include "zep/qt/zepdisplay_qt.h"

namespace Zep
{

class ZepWidget_Qt : public QWidget, public IZepComponent
{
public:
    ZepWidget_Qt(QWidget* pParent, const ZepPath& root, float fontPointSize)
        : QWidget(pParent)
    {
        setFocusPolicy ( Qt::StrongFocus );

        // On Apple/Qt, we scale 1.0 because the OS and Qt take care of the details
#ifdef __APPLE__
        NVec2f pixelScale(1.0f);
#else
        NVec2f pixelScale(NVec2f(qRound(qApp->primaryScreen()->logicalDotsPerInchX() / 96.0f), qRound(qApp->primaryScreen()->logicalDotsPerInchY() / 96.0f)));
#endif

        m_spEditor = std::make_unique<ZepEditor>(new ZepDisplay_Qt(pixelScale), root);
        m_spEditor->RegisterCallback(this);

        auto ptToPx = [](float pt, float dpi) {
            return pt / 72 * dpi;
        };
        float dpi = QGuiApplication::primaryScreen()->logicalDotsPerInch();

        // 14 points, about right for main text area
        m_spEditor->GetDisplay().GetFont(ZepTextType::Text).SetPixelHeight(ptToPx(14, dpi));

        setFocusPolicy(Qt::FocusPolicy::StrongFocus);
        setMouseTracking(true);

        m_refreshTimer.setInterval(1);
        m_refreshTimer.setSingleShot(false);
        m_refreshTimer.start();
        connect(&m_refreshTimer, &QTimer::timeout, this, &ZepWidget_Qt::OnTimer);

    }

    ~ZepWidget_Qt()
    {
        m_spEditor->UnRegisterCallback(this);

        m_spEditor.reset();
    }

    virtual void Notify(std::shared_ptr<ZepMessage> message) override
    {
        if (message->messageId == Msg::RequestQuit)
        {
            qApp->quit();
        }
        else if (message->messageId == Msg::GetClipBoard)
        {
            QClipboard* pClip = QApplication::clipboard();
            message->str = pClip->text().toUtf8().data();
            message->handled = true;
        }
        else if (message->messageId == Msg::SetClipBoard)
        {
            QClipboard* pClip = QApplication::clipboard();
            pClip->setText(QString::fromUtf8(message->str.c_str()));
            message->handled = true;
        }
    }

    virtual void resizeEvent(QResizeEvent* pResize) override
    {
        m_spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(pResize->size().width(), pResize->size().height()));
    }

    virtual void paintEvent(QPaintEvent* pPaint) override
    {
        (void)pPaint;

        QPainter painter(this);
        painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);
        painter.fillRect(rect(), QColor::fromRgbF(.1f, .1f, .1f, 1.0f));

        ((ZepDisplay_Qt&)m_spEditor->GetDisplay()).SetPainter(&painter);

        m_spEditor->Display();

        ((ZepDisplay_Qt&)m_spEditor->GetDisplay()).SetPainter(nullptr);
    }

    virtual void keyPressEvent(QKeyEvent* ev) override
    {
        uint32_t mod = 0;
        auto pMode = m_spEditor->GetActiveTabWindow()->GetActiveWindow()->GetBuffer().GetMode();

        auto isCtrl = [&]()
        {
            // Meta an control swapped on Apple!
            #ifdef __APPLE__
            if (ev->modifiers() & Qt::MetaModifier)
            {
                return true;
            }
            #else
            if (ev->modifiers() & Qt::ControlModifier)
            {
                return true;
            }
            #endif
            return false;
        };

        if (ev->modifiers() & Qt::ShiftModifier)
        {
            mod |= ModifierKey::Shift;
        }
        if (isCtrl())
        {
            mod |= ModifierKey::Ctrl;
            if (ev->key() == Qt::Key_1)
            {
                m_spEditor->SetGlobalMode(ZepMode_Standard::StaticName());
                update();
                return;
            }
            else if (ev->key() == Qt::Key_2)
            {
                m_spEditor->SetGlobalMode(ZepMode_Vim::StaticName());
                update();
                return;
            }
            else
            {
                if (ev->key() >= Qt::Key_A && ev->key() <= Qt::Key_Z)
                {
                    pMode->AddKeyPress((ev->key() - Qt::Key_A) + 'a', mod);
                    update();
                    return;
                }
            }
        }

        if (ev->key() == Qt::Key_Tab)
        {
            pMode->AddKeyPress(ExtKeys::TAB, mod);
        }
        else if (ev->key() == Qt::Key_Escape)
        {
            pMode->AddKeyPress(ExtKeys::ESCAPE, mod);
        }
        else if (ev->key() == Qt::Key_Enter || ev->key() == Qt::Key_Return)
        {
            pMode->AddKeyPress(ExtKeys::RETURN, mod);
        }
        else if (ev->key() == Qt::Key_Delete)
        {
            pMode->AddKeyPress(ExtKeys::DEL, mod);
        }
        else if (ev->key() == Qt::Key_Backspace)
        {
            pMode->AddKeyPress(ExtKeys::BACKSPACE, mod);
        }
        else if (ev->key() == Qt::Key_Home)
        {
            pMode->AddKeyPress(ExtKeys::HOME, mod);
        }
        else if (ev->key() == Qt::Key_End)
        {
            pMode->AddKeyPress(ExtKeys::END, mod);
        }
        else if (ev->key() == Qt::Key_Right)
        {
            pMode->AddKeyPress(ExtKeys::RIGHT, mod);
        }
        else if (ev->key() == Qt::Key_Left)
        {
            pMode->AddKeyPress(ExtKeys::LEFT, mod);
        }
        else if (ev->key() == Qt::Key_Up)
        {
            pMode->AddKeyPress(ExtKeys::UP, mod);
        }
        else if (ev->key() == Qt::Key_Down)
        {
            pMode->AddKeyPress(ExtKeys::DOWN, mod);
        }
        else
        {
            auto input = ev->text().toUtf8();
            if (!input.isEmpty())
            {
                uint8_t* pIn = (uint8_t*)input.data();

                // Convert to UTF8 uint32; not used yet
                uint32_t dw = 0;
                for (int i = 0; i < input.size(); i++)
                {
                    dw |= ((uint32_t)pIn[i]) << ((input.size() - i - 1) * 8);
                }

                pMode->AddKeyPress(dw, mod);
            }
        }
        update();
    }

    ZepMouseButton GetMouseButton(QMouseEvent* ev)
    {
        switch (ev->button())
        {
        case Qt::MouseButton::MiddleButton:
            return ZepMouseButton::Middle;
            break;
        case Qt::MouseButton::LeftButton:
            return ZepMouseButton::Left;
            break;
        case Qt::MouseButton::RightButton:
            return ZepMouseButton::Right;
        default:
            return ZepMouseButton::Unknown;
            break;
        }
    }

    virtual void mousePressEvent(QMouseEvent* ev) override
    {
        if (m_spEditor)
        {
            m_spEditor->OnMouseDown(toNVec2f(ev->localPos()), GetMouseButton(ev));
        }
    }
    virtual void mouseReleaseEvent(QMouseEvent* ev) override
    {
        (void)ev;
        if (m_spEditor)
        {
            m_spEditor->OnMouseUp(toNVec2f(ev->localPos()), GetMouseButton(ev));
        }
    }

    virtual void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        (void)event;
    }

    virtual void mouseMoveEvent(QMouseEvent* ev) override
    {
        if (m_spEditor)
        {
            m_spEditor->OnMouseMove(toNVec2f(ev->localPos()));
        }
    }

    // IZepComponent
    virtual ZepEditor& GetEditor() const override
    {
        return *m_spEditor;
    }

private slots:
    void OnTimer()
    {
        if (m_spEditor->RefreshRequired())
        {
            update();
        }
    }

private:
    std::unique_ptr<ZepEditor> m_spEditor;
    QTimer m_refreshTimer;
};

} // namespace Zep
