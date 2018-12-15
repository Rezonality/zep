#include "display_imgui.h"

// This is an ImGui specific renderer for Zep.  Simple interface for drawing chars, rects, lines.
// Implement a new display for a different rendering type - e.g. terminal or windows Gui.jj
namespace Zep
{

float ZepDisplay_ImGui::GetFontSize() const
{
    return ImGui::GetFontSize();
}

NVec2f ZepDisplay_ImGui::GetTextSize(const utf8* pBegin, const utf8* pEnd) const
{
    // This is the code from ImGui internals; we can't call GetTextSize, because it doesn't return the correct 'advance' formula, which we
    // need as we draw one character at a time...
    ImFont* font = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize();
    ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX, (const char*)pBegin, (const char*)pEnd, NULL);

    return toNVec2f(text_size);
}

void ZepDisplay_ImGui::DrawChars(const NVec2f& pos, uint32_t col, const utf8* text_begin, const utf8* text_end) const
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (text_end == nullptr)
    {
        text_end = text_begin + strlen((const char*)text_begin);
    }
    drawList->AddText(toImVec2(pos), col, (const char*)text_begin, (const char*)text_end);
}

void ZepDisplay_ImGui::DrawLine(const NVec2f& start, const NVec2f& end, uint32_t color, float width) const
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    // Background rect for numbers
    drawList->AddLine(toImVec2(start), toImVec2(end), color, width);
}

void ZepDisplay_ImGui::DrawRectFilled(const NVec2f& a, const NVec2f& b, uint32_t color) const
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    // Background rect for numbers
    drawList->AddRectFilled(toImVec2(a), toImVec2(b), color);
}


} // Zep
