#pragma once

#include "buffer.h"

namespace Zep
{

class ZepTabWindow;

// A region inside the text for selections
struct SelectRegion
{
    // For vertical select, we will have a list of spans...
    BufferLocation start = 0;
    BufferLocation end = 0;
    bool visible = true;
    bool vertical = false; // Not yet supported
};

// Display interface
class ZepDisplay
{
public:
    virtual ~ZepDisplay(){};

    // Renderer specific overrides
    // Implement these to draw the buffer using whichever system you prefer
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const = 0;
    virtual float GetFontPointSize() const = 0;
    virtual void SetFontPointSize(float size)
    {
        (void)size;
    };
    virtual float GetFontHeightPixels() const = 0;
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color = NVec4f(1.0f), float width = 1.0f) const = 0;
    virtual void DrawChars(const NVec2f& pos, const NVec4f& col, const utf8* text_begin, const utf8* text_end = nullptr) const = 0;
    virtual void DrawRectFilled(const NRectf& rc, const NVec4f& col = NVec4f(1.0f)) const = 0;
    virtual void SetClipRect(const NRectf& rc) = 0;

    virtual NVec2f GetCharSize(const utf8* pChar);
    virtual const NVec2f& GetDefaultCharSize();
    virtual void InvalidateCharCache();

protected:
    void BuildCharCache();

protected:
    bool m_charCacheDirty = true;
    NVec2f m_charCache[256];
    NVec2f m_defaultCharSize;
};

// A NULL renderer, used for testing
// Discards all drawing, and returns text fixed_size of 1 pixel per char, 10 height!
// This is the only work you need to do to make a new renderer type for the editor
class ZepDisplayNull : public ZepDisplay
{
public:
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const override
    {
        return NVec2f(float(pEnd - pBegin), 10.0f);
    }
    virtual float GetFontPointSize() const override
    {
        return 10;
    }
    virtual float GetFontHeightPixels() const override
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
    virtual void DrawChars(const NVec2f& pos, const NVec4f& col, const utf8* text_begin, const utf8* text_end = nullptr) const override
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
