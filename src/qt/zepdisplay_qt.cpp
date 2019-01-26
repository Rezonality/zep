#include "zepdisplay_qt.h"
#include <qapplication.h>
#if _WIN32
#include <windows.h>
#endif

// This is an ImGui specific renderer for Zep.  Simple interface for drawing chars, rects, lines.
// Implement a new display for a different rendering type - e.g. terminal or windows Gui.jj
namespace Zep
{

ZepDisplay_Qt::ZepDisplay_Qt()
{
    qApp->setFont(QFont("Consolas", 10));

    QFontMetrics met(qApp->font());
    m_fontSize = met.height();
    m_fontOffset = met.ascent();
}

ZepDisplay_Qt::~ZepDisplay_Qt()
{
}

float ZepDisplay_Qt::GetFontSize() const
{
    return m_fontSize;
}

NVec2f ZepDisplay_Qt::GetTextSize(const utf8* pBegin, const utf8* pEnd) const
{
    QFontMetrics met(qApp->font());
    if (pEnd == nullptr)
    {
        pEnd = pBegin + strlen((const char*)pBegin);
    }
    
    auto rc = met.size(Qt::TextSingleLine, QString::fromUtf8((char*)pBegin, pEnd - pBegin));
    return NVec2f(rc.width(), rc.height());
}

void ZepDisplay_Qt::DrawChars(const NVec2f& pos, const NVec4f& col, const utf8* text_begin, const utf8* text_end) const
{
    if (text_end == nullptr)
    {
        text_end = text_begin + strlen((const char*)text_begin);
    }
    QPoint p0 = toQPoint(pos);
    m_pPainter->setPen(QColor::fromRgbF(col.x, col.y, col.z, col.w));

    p0.setY(p0.y() + m_fontOffset);

    m_pPainter->drawText(p0, QString::fromUtf8((char*)text_begin, text_end - text_begin));
}

void ZepDisplay_Qt::DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color, float width) const
{
    QPoint p0 = toQPoint(start);
    QPoint p1 = toQPoint(end);
    m_pPainter->setPen(QPen(QBrush(QColor::fromRgbF(color.x, color.y, color.z, color.w)), width));
    m_pPainter->drawLine(p0, p1);
}

void ZepDisplay_Qt::DrawRectFilled(const NRectf& a, const NVec4f& color) const
{
    QPoint start = toQPoint(a.topLeftPx);
    QPoint end = toQPoint(a.bottomRightPx);
    m_pPainter->fillRect(QRect(start, end), QColor::fromRgbF(color.x, color.y, color.z, color.w));
}

void ZepDisplay_Qt::SetClipRect(const NRectf& rc)
{
    m_clipRect = rc;
    if (m_clipRect.Width() > 0)
    {
        auto clip = QRect(m_clipRect.topLeftPx.x, m_clipRect.topLeftPx.y, m_clipRect.Width(), m_clipRect.Height());
        m_pPainter->setClipRect(clip);
    }
    else
    {
        m_pPainter->setClipping(false);
    }
}

} // namespace Zep
