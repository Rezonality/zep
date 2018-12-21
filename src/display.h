#pragma once

#include "buffer.h"

namespace Zep
{

class ZepTabWindow;

// A region inside the text for selections
struct Region
{
    // For vertical select, we will have a list of spans...
    BufferLocation start;  
    BufferLocation end;
    bool visible;
    bool vertical;      // Not yet supported
};

// Display interface
class IZepDisplay
{
public:
    virtual ~IZepDisplay() {};

    // Renderer specific overrides
    // Implement these to draw the buffer using whichever system you prefer
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const = 0;
    virtual float GetFontSize() const = 0;
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, uint32_t color = 0xFFFFFFFF, float width = 1.0f) const = 0;
    virtual void DrawChars(const NVec2f& pos, uint32_t col, const utf8* text_begin, const utf8* text_end = nullptr) const = 0;
    virtual void DrawRectFilled(const NVec2f& a, const NVec2f& b, uint32_t col = 0xFFFFFFFF) const = 0;
    virtual void SetClipRect(const DisplayRegion& rc) = 0;
};

// A NULL renderer, used for testing
// Discards all drawing, and returns text size of 1 pixel per char, 10 height!
// This is the only work you need to do to make a new renderer type for the editor
class ZepDisplayNull : public IZepDisplay
{
public:
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const override { return NVec2f(float(pEnd - pBegin), 10.0f); }
    virtual float GetFontSize() const override { return 10; }
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, uint32_t color = 0xFFFFFFFF, float width = 1.0f) const override { };
    virtual void DrawChars(const NVec2f& pos, uint32_t col, const utf8* text_begin, const utf8* text_end = nullptr) const override { };
    virtual void DrawRectFilled(const NVec2f& a, const NVec2f& b, uint32_t col = 0xFFFFFFFF) const override {};
    virtual void SetClipRect(const DisplayRegion& rc) override {}

};

} // Zep
