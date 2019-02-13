#pragma once
#include <string>
#include "display.h"
#include "syntax.h"
#include <imgui.h>

namespace Zep
{

inline NVec2f toNVec2f(const ImVec2& im)
{
    return NVec2f(im.x, im.y);
}
inline ImVec2 toImVec2(const NVec2f& im)
{
    return ImVec2(im.x, im.y);
}

class ZepDisplay_ImGui : public ZepDisplay
{
public:
    // ImGui specific display methods
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const override;
    virtual float GetFontPointSize() const override;
    virtual void SetFontPointSize(float size) override;
    virtual float GetFontHeightPixels() const override;
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color = NVec4f(1.0f), float width = 1.0f) const override;
    virtual void DrawChars(const NVec2f& pos, const NVec4f& col, const utf8* text_begin, const utf8* text_end = nullptr) const override;
    virtual void DrawRectFilled(const NRectf& a, const NVec4f& col = NVec4f(1.0f)) const override;
    virtual void SetClipRect(const NRectf& a) override;

private:
    NRectf m_clipRect;
};

} // namespace Zep
