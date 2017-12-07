#include "display_qt.h"
#include <qapplication.h>

// This is an ImGui specific renderer for Zep.  Simple interface for drawing chars, rects, lines.
// Implement a new display for a different rendering type - e.g. terminal or windows Gui.jj
namespace Zep
{

ZepDisplay_Qt::ZepDisplay_Qt(ZepEditor& editor)
    : TParent(editor)
{
    qApp->setFont(QFont("Consolas", 9));
}

ZepDisplay_Qt::~ZepDisplay_Qt()
{
}

float ZepDisplay_Qt::GetFontSize() const
{
    QFontMetrics met(qApp->font());
    return met.height();
}

NVec2f ZepDisplay_Qt::GetTextSize(const utf8* pBegin, const utf8* pEnd) const
{
    QFontMetrics met(qApp->font());
    if (pEnd == nullptr)
    {
        pEnd = pBegin + strlen((const char*)pBegin);
    }
    auto rc = met.size(Qt::TextSingleLine, QString::fromUtf8((char*)pBegin, pEnd- pBegin));
    return NVec2f(rc.width(), rc.height());
}

void ZepDisplay_Qt::DrawChars(const NVec2f& pos, uint32_t col, const utf8* text_begin, const utf8* text_end) const
{
    if (text_end == nullptr)
    {
        text_end = text_begin + strlen((const char*)text_begin);
    }
    QPoint p0 = toQPoint(pos);
    uint32_t color = (qAlpha(col) << 24) | (qRed(col)) | (qGreen(col) << 8) | (qBlue(col) << 16);
    m_pPainter->setPen(QColor::fromRgba(color));
    p0.setY(p0.y() + GetFontSize());
    m_pPainter->drawText(p0, QString::fromUtf8((char*)text_begin, text_end - text_begin));
}

void ZepDisplay_Qt::DrawLine(const NVec2f& start, const NVec2f& end, uint32_t color, float width) const
{
    QPoint p0 = toQPoint(start);
    QPoint p1 = toQPoint(end);
    m_pPainter->setPen(QPen(QBrush(QColor::fromRgba(color)), width));
    m_pPainter->drawLine(p0, p1);
}

void ZepDisplay_Qt::DrawRectFilled(const NVec2f& a, const NVec2f& b, uint32_t color) const
{
    QPoint start = toQPoint(a);
    QPoint end = toQPoint(b);
    m_pPainter->fillRect(QRect(start, end), QColor::fromRgba(color));
}

} // Zep
