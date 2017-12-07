#pragma once
#include <string>
#include "display.h"
#include "syntax.h"
#include <imgui.h>

namespace Zep
{

inline NVec2f toNVec2f(const ImVec2& im) { return NVec2f(im.x, im.y); }
inline ImVec2 toImVec2(const NVec2f& im) { return ImVec2(im.x, im.y); }

class ZepDisplay_ImGui : public ZepDisplay
{
public:
    using TParent = ZepDisplay;
    ZepDisplay_ImGui(ZepEditor& editor);
    ~ZepDisplay_ImGui();

    // ImGui specific display methods
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const override;
    virtual float GetFontSize() const override;
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, uint32_t color = 0xFFFFFFFF, float width = 1.0f) const override;
    virtual void DrawChars(const NVec2f& pos, uint32_t col, const utf8* text_begin, const utf8* text_end = nullptr) const override;
    virtual void DrawRectFilled(const NVec2f& a, const NVec2f& b, uint32_t col = 0xFFFFFFFF) const override;
private:
};

} // Zep
