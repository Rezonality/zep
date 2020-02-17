#pragma once
#include <string>

#include "../display.h"
#include "../syntax.h"

// Can't include this publicly
//#include "zep/mcommon/logger.h"

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
    float GetFontPointSize() const
    {
        return ImGui::GetFontSize();
    }

    void SetFontPointSize(float size)
    {
        // TODO: Allow change of font size in ImGui
        // See the Qt demo for functional font size changing with CTRL+/-
        (void)size;
    }

    float GetFontHeightPixels() const
    {
        // TODO: Check low DPI monitor for correct behavior
        return ImGui::GetFontSize();
    }

    NVec2f GetTextSize(const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const
    {
        // This is the code from ImGui internals; we can't call GetTextSize, because it doesn't return the correct 'advance' formula, which we
        // need as we draw one character at a time...
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX, (const char*)pBegin, (const char*)pEnd, NULL);
        if (text_size.x == 0.0)
        {
            // Make invalid characters a default fixed_size
            return GetTextSize((const uint8_t*)"A");
        }

        return toNVec2f(text_size);
    }

    void DrawChars(const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (text_end == nullptr)
        {
            text_end = text_begin + strlen((const char*)text_begin);
        }
        if (m_clipRect.Width() == 0)
        {
            drawList->AddText(toImVec2(pos), ToPackedABGR(col), (const char*)text_begin, (const char*)text_end);
        }
        else
        {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddText(toImVec2(pos), ToPackedABGR(col), (const char*)text_begin, (const char*)text_end);
            drawList->PopClipRect();
        }
    }

    void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color, float width) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // Background rect for numbers
        if (m_clipRect.Width() == 0)
        {
            drawList->AddLine(toImVec2(start), toImVec2(end), ToPackedABGR(color), width);
        }
        else
        {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddLine(toImVec2(start), toImVec2(end), ToPackedABGR(color), width);
            drawList->PopClipRect();
        }
    }

    void DrawRectFilled(const NRectf& rc, const NVec4f& color) const
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        // Background rect for numbers
        if (m_clipRect.Width() == 0)
        {
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), ToPackedABGR(color));
        }
        else
        {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddRectFilled(toImVec2(rc.topLeftPx), toImVec2(rc.bottomRightPx), ToPackedABGR(color));
            drawList->PopClipRect();
        }
        //LOG(INFO) << "Draw: RC: " << rc << ", Color:" << color;
    }

    void SetClipRect(const NRectf& rc)
    {
        m_clipRect = rc;
    }

private:
    NRectf m_clipRect;
};

} // namespace Zep
