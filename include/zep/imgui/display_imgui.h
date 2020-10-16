#pragma once
#include "../display.h"
#include "../syntax.h"
#include <imgui/imgui.h>
#include <string>

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

static ImWchar greek_range[] = { 0x300, 0x52F, 0x1f00, 0x1fff, 0, 0 };

class ZepFont_ImGui : public ZepFont
{
public:
    ZepFont_ImGui(ZepDisplay& display, ImFont* pFont, int pixelHeight)
        : ZepFont(display)
        , m_pFont(pFont)
    {
        SetPixelHeight(pixelHeight);
    }

    virtual void SetPixelHeight(int pixelHeight) override
    {
        InvalidateCharCache();
        m_pixelHeight = pixelHeight;
    }

    virtual NVec2f GetTextSize(const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const override
    {
        // This is the code from ImGui internals; we can't call GetTextSize, because it doesn't return the correct 'advance' formula, which we
        // need as we draw one character at a time...
        const float font_size = m_pFont->FontSize;
        ImVec2 text_size = m_pFont->CalcTextSizeA(float(GetPixelHeight()), FLT_MAX, FLT_MAX, (const char*)pBegin, (const char*)pEnd, NULL);
        if (text_size.x == 0.0)
        {
            // Make invalid characters a default fixed_size
            const char chDefault = 'A';
            text_size = m_pFont->CalcTextSizeA(float(GetPixelHeight()), FLT_MAX, FLT_MAX, &chDefault, (&chDefault + 1), NULL);
        }

        return toNVec2f(text_size);
    }

    ImFont* GetImFont()
    {
        return m_pFont;
    }

private:
    ImFont* m_pFont;
    float m_fontScale = 1.0f;
};

class ZepDisplay_ImGui : public ZepDisplay
{
public:
    ZepDisplay_ImGui(const NVec2f& pixelScale)
        : ZepDisplay(pixelScale)
    {
    }

    void DrawChars(ZepFont& font, const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end) const
    {
        auto imFont = static_cast<ZepFont_ImGui&>(font).GetImFont();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        if (text_end == nullptr)
        {
            text_end = text_begin + strlen((const char*)text_begin);
        }
        if (m_clipRect.Width() == 0)
        {
            drawList->AddText(imFont, float(font.GetPixelHeight()), toImVec2(pos), ToPackedABGR(col), (const char*)text_begin, (const char*)text_end);
        }
        else
        {
            drawList->PushClipRect(toImVec2(m_clipRect.topLeftPx), toImVec2(m_clipRect.bottomRightPx));
            drawList->AddText(imFont, float(font.GetPixelHeight()), toImVec2(pos), ToPackedABGR(col), (const char*)text_begin, (const char*)text_end);
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

    virtual void SetClipRect(const NRectf& rc) override
    {
        m_clipRect = rc;
    }

    virtual ZepFont& GetFont(ZepTextType type) override
    {
        if (m_fonts[(int)type] == nullptr)
        {
            m_fonts[(int)type] = std::make_shared<ZepFont_ImGui>(*this, ImGui::GetFont(), int(16.0f * GetPixelScale().y));
        }
        return *m_fonts[(int)type];
    }


private:
    NRectf m_clipRect;
}; // namespace Zep

} // namespace Zep
