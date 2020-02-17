#pragma once

#include <QApplication>
#include <QPainter>
#if _WIN32
#include <windows.h>
#endif

#include "zep/qt/common_qt.h"
#include "zep/display.h"
#include "zep/syntax.h"
#include <string>

namespace Zep
{

inline NVec2f toNVec2f(const QPoint& im)
{
    return NVec2f(im.x(), im.y());
}
inline NVec2f toNVec2f(const QPointF& im)
{
    return NVec2f(im.x(), im.y());
}
inline QPoint toQPoint(const NVec2f& im)
{
    return QPoint(im.x, im.y);
}

class ZepDisplay_Qt : public ZepDisplay
{
public:
    using TParent = ZepDisplay;

    void SetPainter(QPainter* pPainter)
    {
        m_pPainter = pPainter;
    }

    ZepDisplay_Qt()
    {
        SetFontPointSize(10.0f);
    }

    ~ZepDisplay_Qt()
    {
    }

    void SetFontPointSize(float fVal)
    {
        qApp->setFont(QFont("Consolas", fVal));
        QFontMetrics met(qApp->font());
        m_fontOffset = met.ascent();
        m_fontHeight = (float)met.height();
        InvalidateCharCache();
    }

    float GetFontPointSize() const
    {
        return DPI::GetFontPointSize();
    }

    float GetFontHeightPixels() const
    {
        return m_fontHeight;
    }

    NVec2f GetTextSize(const uint8_t* pBegin, const uint8_t* pEnd) const
    {
        QFontMetrics met(qApp->font());
        if (pEnd == nullptr)
        {
            pEnd = pBegin + strlen((const char*)pBegin);
        }

        auto rc = met.size(Qt::TextSingleLine, QString::fromUtf8((char*)pBegin, pEnd - pBegin));
        return NVec2f(rc.width(), rc.height());
    }

    void DrawChars(const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end) const
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

    void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color, float width) const
    {
        QPoint p0 = toQPoint(start);
        QPoint p1 = toQPoint(end);
        m_pPainter->setPen(QPen(QBrush(QColor::fromRgbF(color.x, color.y, color.z, color.w)), width));
        m_pPainter->drawLine(p0, p1);
    }

    void DrawRectFilled(const NRectf& a, const NVec4f& color) const
    {
        QPoint start = toQPoint(a.topLeftPx);
        QPoint end = toQPoint(a.bottomRightPx);
        m_pPainter->fillRect(QRect(start, end), QColor::fromRgbF(color.x, color.y, color.z, color.w));
    }

    void SetClipRect(const NRectf& rc)
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

private:
    QPainter* m_pPainter = nullptr;
    int m_fontOffset;
    float m_fontHeight;
    NRectf m_clipRect;
};

} // namespace Zep
