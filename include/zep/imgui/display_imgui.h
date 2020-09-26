#pragma once
#include <string>
#include <imgui/imgui.h>
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
    float GetFontPointSize(ZepFontType type) const
    {
        ZEP_UNUSED(type);
        return ImGui::GetFontSize() * ImGui::GetIO().FontGlobalScale;
    }

    /*void AddFont(const std::string& fontName, float pointSize)
    ImVector<ImWchar> ranges;
    ImFontGlyphRangesBuilder builder;
    builder.AddRanges(io.Fonts->GetGlyphRangesDefault()); // Add one of the default ranges
    builder.AddRanges(io.Fonts->GetGlyphRangesCyrillic()); // Add one of the default ranges
    //builder.AddRanges(io.Fonts->GetGlyphRangesThai()); // Add one of the default ranges
    ImWchar greek_range[] = { 0x300, 0x52F, 0x1f00, 0x1fff, 0, 0 };
    builder.AddRanges(greek_range);
    builder.BuildRanges(&ranges); // Build the final result (ordered ranges with all the unique characters submitted)

    ImFontConfig cfg;
    cfg.OversampleH = 4;
    cfg.OversampleV = 4;

    float fontPixelHeight = dpi_pixel_height_from_point_size(DemoFontPtSize, GetDisplayScale().y);
    io.Fonts->AddFontFromFileTTF((std::string(SDL_GetBasePath()) + "Cousine-Regular.ttf").c_str(), fontPixelHeight, &cfg, ranges.Data);
    */

    void SetFontPointSize(ZepFontType type, float size)
    {
        ZEP_UNUSED(type);
        // A crude scaling in ImGui for now...
        // We use global font scale instead of doing it 'properly'
        // See the Qt demo for better scaling, because that's built into Qt.
        GetFontCache(type).charCacheDirty = true;
        ImGui::GetIO().FontGlobalScale = size / ImGui::GetFontSize();
    }

    float GetFontHeightPixels(ZepFontType type) const
    {
        ZEP_UNUSED(type);
        return ImGui::GetFontSize();
        // So Qt claims to return the below; but I've been unable to get similar fonts to match.
        // There is much more padding in Qt than in ImGui, even though the actual font sizes are the same!
        //return (ImGui::GetFont()->Descent + ImGui::GetFont()->Ascent + 1) * 2.0f;
    }

    NVec2f GetTextSize(ZepFontType type, const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const
    {
        ZEP_UNUSED(type);
        // This is the code from ImGui internals; we can't call GetTextSize, because it doesn't return the correct 'advance' formula, which we
        // need as we draw one character at a time...
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX, (const char*)pBegin, (const char*)pEnd, NULL);
        if (text_size.x == 0.0)
        {
            // Make invalid characters a default fixed_size
            const char chDefault = 'A';
            text_size = font->CalcTextSizeA(font_size, FLT_MAX, FLT_MAX, &chDefault, (&chDefault + 1), NULL);
        }

        return toNVec2f(text_size);
    }

    void DrawChars(ZepFontType type, const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end) const
    {
        ZEP_UNUSED(type);
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
    }

    void SetClipRect(const NRectf& rc)
    {
        m_clipRect = rc;
    }

private:
    NRectf m_clipRect;
    float m_fontScale = 1.0f;
};

} // namespace Zep
