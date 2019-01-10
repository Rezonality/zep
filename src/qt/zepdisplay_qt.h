#pragma once
#include <string>
#include "display.h"
#include "syntax.h"

#include <QPainter>
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

class ZepDisplay_Qt : public IZepDisplay
{
public:
    using TParent = IZepDisplay;
    ZepDisplay_Qt();
    virtual ~ZepDisplay_Qt();

    void SetPainter(QPainter* pPainter)
    {
        m_pPainter = pPainter;
    }

    // ImGui specific display methods
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const override;
    virtual float GetFontSize() const override;
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color = NVec4f(1.0f), float width = 1.0f) const override;
    virtual void DrawChars(const NVec2f& pos, const NVec4f& col, const utf8* text_begin, const utf8* text_end = nullptr) const override;
    virtual void DrawRectFilled(const NRectf& rc, const NVec4f& col = NVec4f(1.0f)) const override;
    virtual void SetClipRect(const NRectf& rc) override;

private:
    QPainter* m_pPainter = nullptr;
    int m_fontSize;
    int m_fontOffset;
    NRectf m_clipRect;
};

} // namespace Zep
