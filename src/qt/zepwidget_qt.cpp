#include <QApplication>
#include <QKeyEvent>
#include <QDesktopWidget>
#include <string>

#include "editor.h"
#include "logger.h"
#include "mode.h"
#include "mode_standard.h"
#include "mode_vim.h"
#include "tab_window.h"

#include "zepdisplay_qt.h"
#include "zepwidget_qt.h"

namespace Zep
{

ZepWidget_Qt::ZepWidget_Qt(QWidget* pParent, const fs::path& root)
    : QWidget(pParent)
{
    m_spEditor = std::make_unique<ZepEditor>(new ZepDisplay_Qt(), root);
    m_spEditor->RegisterCallback(this);
    m_spEditor->SetPixelScale(float(qApp->desktop()->logicalDpiX() / 96.0f));

    setFocusPolicy(Qt::FocusPolicy::StrongFocus);
    setMouseTracking(true);

    m_refreshTimer.setInterval(1);
    m_refreshTimer.setSingleShot(false);
    m_refreshTimer.start();
    connect(&m_refreshTimer, &QTimer::timeout, this, &ZepWidget_Qt::OnTimer);
}

ZepWidget_Qt::~ZepWidget_Qt()
{
    m_spEditor->UnRegisterCallback(this);

    m_spEditor.reset();
}

void ZepWidget_Qt::Notify(std::shared_ptr<ZepMessage> message)
{
    if (message->messageId == Msg::Quit)
    {
        qApp->quit();
    }
}

void ZepWidget_Qt::OnTimer()
{
    if (m_spEditor->RefreshRequired())
    {
        update();
    }
}

void ZepWidget_Qt::resizeEvent(QResizeEvent* pResize)
{
    m_spEditor->SetDisplayRegion(NVec2f(0.0f, 0.0f), NVec2f(pResize->size().width(), pResize->size().height()));
}

void ZepWidget_Qt::paintEvent(QPaintEvent* pPaint)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::RenderHint::Antialiasing, true);
    painter.fillRect(rect(), QColor::fromRgbF(.1f, .1f, .1f, 1.0f));

    ((ZepDisplay_Qt&)m_spEditor->GetDisplay()).SetPainter(&painter);

    m_spEditor->Display();

    ((ZepDisplay_Qt&)m_spEditor->GetDisplay()).SetPainter(nullptr);
}

void ZepWidget_Qt::keyPressEvent(QKeyEvent* ev)
{
    uint32_t mod = 0;
    auto pMode = m_spEditor->GetCurrentMode();

    if (ev->modifiers() & Qt::ShiftModifier)
    {
        mod |= ModifierKey::Shift;
    }
    if (ev->modifiers() & Qt::ControlModifier)
    {
        mod |= ModifierKey::Ctrl;
        if (ev->key() == Qt::Key_1)
        {
            m_spEditor->SetMode(ZepMode_Standard::StaticName());
            update();
            return;
        }
        else if (ev->key() == Qt::Key_2)
        {
            m_spEditor->SetMode(ZepMode_Vim::StaticName());
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
        QString input = ev->text();
        for (auto& i : input)
        {
            auto ch = i.toLatin1();
            if (ch != 0)
            {
                m_spEditor->GetCurrentMode()->AddKeyPress(i.toLatin1(), mod);
            }
        }
    }
    update();
}

ZepMouseButton ZepWidget_Qt::GetMouseButton(QMouseEvent* ev)
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

void ZepWidget_Qt::mousePressEvent(QMouseEvent *ev)
{
    if (m_spEditor)
    {
        m_spEditor->OnMouseDown(toNVec2f(ev->localPos()), GetMouseButton(ev));
    }
}
void ZepWidget_Qt::mouseReleaseEvent(QMouseEvent *ev)
{
    if (m_spEditor)
    {
        m_spEditor->OnMouseUp(toNVec2f(ev->localPos()), GetMouseButton(ev));
    }
}

void ZepWidget_Qt::mouseDoubleClickEvent(QMouseEvent *event)
{
}

void ZepWidget_Qt::mouseMoveEvent(QMouseEvent *ev)
{
    if (m_spEditor)
    {
        m_spEditor->OnMouseMove(toNVec2f(ev->localPos()));
    }
}
} // namespace Zep
