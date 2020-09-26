#pragma once

#include <array>
#include "buffer.h"

namespace Zep
{

class ZepTabWindow;

// A region inside the text for selections
struct SelectRegion
{
    // For vertical select, we will have a list of spans...
    GlyphIterator start;
    GlyphIterator end;
    bool visible = true;
    bool vertical = false; // Not yet supported
};

using ZepFontHandle = uint32_t;

enum class ZepFontType
{
    UI,
    Text,
    Heading1,
    Heading2,
    Heading3,
    Count
};

// Display interface
class ZepDisplay
{
public:
    virtual ~ZepDisplay(){};

    // Renderer specific overrides
    // Implement these to draw the buffer using whichever system you prefer
    virtual NVec2f GetTextSize(ZepFontType type, const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const = 0;
    virtual float GetFontPointSize(ZepFontType type) const = 0;
    virtual float GetFontHeightPixels(ZepFontType type) const = 0;
    virtual void SetFontPointSize(ZepFontType, float) { };

    virtual void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color = NVec4f(1.0f), float width = 1.0f) const = 0;
    virtual void DrawChars(ZepFontType type, const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end = nullptr) const = 0;
    virtual void DrawRectFilled(const NRectf& rc, const NVec4f& col = NVec4f(1.0f)) const = 0;
    virtual void SetClipRect(const NRectf& rc) = 0;

    virtual uint32_t GetCodePointCount(const uint8_t* pCh, const uint8_t* pEnd) const;
    virtual NVec2f GetCharSize(ZepFontType type, const uint8_t* pChar);
    virtual const NVec2f& GetDefaultCharSize(ZepFontType type);
    virtual const NVec2f& GetDotSize(ZepFontType type);
    virtual void InvalidateCharCache(ZepFontType type);
    virtual void DrawRect(const NRectf& rc, const NVec4f& col = NVec4f(1.0f)) const;

protected:
    void BuildCharCache(ZepFontType type);

protected:
    struct FontTypeCache
    {
        bool charCacheDirty = true;
        std::unordered_map<uint32_t, NVec2f> charCache;
        NVec2f charCacheASCII[256];
        NVec2f dotSize;
        NVec2f defaultCharSize;
    };
    std::array<FontTypeCache, size_t(ZepFontType::Count)> m_fontCache;
    
    FontTypeCache& GetFontCache(ZepFontType fontType);
};

// A NULL renderer, used for testing
// Discards all drawing, and returns text fixed_size of 1 pixel per char, 10 height!
// This is the only work you need to do to make a new renderer type for the editor
class ZepDisplayNull : public ZepDisplay
{
public:
    virtual NVec2f GetTextSize(ZepFontType, const uint8_t* pBegin, const uint8_t* pEnd = nullptr) const override
    {
        return NVec2f(float(pEnd - pBegin), 10.0f);
    }
    virtual float GetFontPointSize(ZepFontType) const override
    {
        return 10;
    }
    virtual float GetFontHeightPixels(ZepFontType) const override
    {
        return 10;
    }
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color = NVec4f(1.0f), float width = 1.0f) const override
    {
        (void)start;
        (void)end;
        (void)color;
        (void)width;
    };
    virtual void DrawChars(ZepFontType, const NVec2f& pos, const NVec4f& col, const uint8_t* text_begin, const uint8_t* text_end = nullptr) const override
    {
        (void)pos;
        (void)col;
        (void)text_begin;
        (void)text_end;
    }
    virtual void DrawRectFilled(const NRectf& a, const NVec4f& col = NVec4f(1.0f)) const override
    {
        (void)a;
        (void)col;
    };
    virtual void SetClipRect(const NRectf& rc) override
    {
        (void)rc;
    }
};

} // namespace Zep
