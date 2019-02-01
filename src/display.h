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
class IZepDisplay
{
public:
    virtual ~IZepDisplay(){};

    // Renderer specific overrides
    // Implement these to draw the buffer using whichever system you prefer
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const = 0;
    virtual float GetFontSize() const = 0;
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color = NVec4f(1.0f), float width = 1.0f) const = 0;
    virtual void DrawChars(const NVec2f& pos, const NVec4f& col, const utf8* text_begin, const utf8* text_end = nullptr) const = 0;
    virtual void DrawRectFilled(const NRectf& rc, const NVec4f& col = NVec4f(1.0f)) const = 0;
    virtual void SetClipRect(const NRectf& rc) = 0;
};

// A NULL renderer, used for testing
// Discards all drawing, and returns text fixed_size of 1 pixel per char, 10 height!
// This is the only work you need to do to make a new renderer type for the editor
class ZepDisplayNull : public IZepDisplay
{
public:
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const override
    {
        return NVec2f(float(pEnd - pBegin), 10.0f);
    }
    virtual float GetFontSize() const override
    {
        return 10;
    }
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, const NVec4f& color = NVec4f(1.0f), float width = 1.0f) const override
    {
        start;
        end;
        color;
        width;
    };
    virtual void DrawChars(const NVec2f& pos, const NVec4f& col, const utf8* text_begin, const utf8* text_end = nullptr) const override
    {
        pos;
        col;
        text_begin;
        text_end;
    }
    virtual void DrawRectFilled(const NRectf& a, const NVec4f& col = NVec4f(1.0f)) const override
    {
        a;
        col;

    };
    virtual void SetClipRect(const NRectf& rc) override
    {
        rc;
    }
};

} // namespace Zep
