#pragma once

#include <QApplication>
#include <QPainter>
#if _WIN32
#include <windows.h>
#endif

#include "zep/qt/common_qt.h"
#include "zep/display.h"
#include "zep/syntax.h"
#include "zep/editor.h"
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

class ZepFont_Qt : public ZepFont
{
public:
    ZepFont_Qt(ZepDisplay& display, const std::string& filePath, int pixelHeight)
        : ZepFont(display)
    {
        SetPixelHeight(pixelHeight);
    }

    virtual void SetPixelHeight(int val) override
    {
        #ifdef __APPLE__
        m_font = QFont("Menlo");
        #else
        m_font = QFont("Consolas");
        #endif
        m_font.setStyleHint(QFont::Monospace);
        m_font.setPixelSize(val);
        m_pixelHeight = val;

        QFontMetrics met(m_font);
        m_descent = met.descent();

        InvalidateCharCache();
    }

    virtual NVec2f GetTextSize(const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const override
    {
        QFontMetrics met(m_font);
        if (pEnd == nullptr)
        {
            pEnd = pBegin + strlen((const char*)pBegin);
        }

        auto rc = met.size(Qt::TextIncludeTrailingSpaces | Qt::TextLongestVariant, QString::fromUtf8((char*)pBegin, pEnd - pBegin));
        if (*pBegin == '\t' && (pEnd == (pBegin + 1)))
        {
            // Default tab width
            rc.setWidth(rc.width() * 4);
        }

        if (rc.width() == 0.0)
        {
            // Make invalid characters a default fixed_size
            const char chDefault = 'A';
            rc = met.size(Qt::TextIncludeTrailingSpaces | Qt::TextLongestVariant, QString("A"));

        }
        return NVec2f(rc.width(), rc.height());
    }

    QFont& GetQtFont()
    {
        return m_font;
    }

    float Descent() const
    {
        return m_descent;
    }

private:
    float m_fontScale = 1.0f;
    float m_descent = 0.0f;
    QFont m_font;
};

class ZepDisplay_Qt : public ZepDisplay
{
public:
    using TParent = ZepDisplay;

    ZepDisplay_Qt(const NVec2f& pixelScale)
        : ZepDisplay(pixelScale)
    {
    }

    ~ZepDisplay_Qt()
    {
    }

    void SetPainter(QPainter* pPainter)
    {
        m_pPainter = pPainter;
    }

    void DrawChars(ZepFont& font, const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end) const
    {
        if (text_end == nullptr)
        {
            text_end = text_begin + strlen((const char*)text_begin);
        }
        auto& qtFont = static_cast<ZepFont_Qt&>(font);
        m_pPainter->setFont(qtFont.GetQtFont());
        QPoint p0 = toQPoint(pos);
        m_pPainter->setPen(QColor::fromRgbF(col.x, col.y, col.z, col.w));

        // Note: this logic works, but is suspect; I had trouble getting the font to align with where it should be placed!
        //m_pPainter->drawText(p0.x(), p0.y() + font.GetPixelHeight() - qtFont.Descent() + 1, QString::fromUtf8((char*)text_begin, text_end - text_begin));

        m_pPainter->drawText(p0.x(), p0.y() - qtFont.Descent() + GetPixelScale().y * 1, m_pPainter->viewport().width() - p0.x(), m_pPainter->viewport().height() - p0.y(), Qt::TextLongestVariant, QString::fromUtf8((char*)text_begin, text_end - text_begin));
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

    virtual void SetClipRect(const NRectf& rc) override
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
    
    virtual ZepFont& GetFont(ZepTextType type) override
    {
        if (m_fonts[(int)type] == nullptr)
        {
            QFontMetrics met(qApp->font());
            auto height = met.height();
            switch (type)
            {
            case ZepTextType::Heading1:
                height *= 1.75f;
                break;
            case ZepTextType::Heading2:
                height *= 1.5f;
                break;
            case ZepTextType::Heading3:
                height *= 1.25f;
                break;
            }
            m_fonts[(int)type] = std::make_shared<ZepFont_Qt>(*this, "", height);
        }
        return *m_fonts[(int)type];
    }

private:
    QPainter* m_pPainter = nullptr;
    NRectf m_clipRect;
};

} // namespace Zep
