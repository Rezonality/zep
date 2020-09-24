#include "zep/editor.h"
#include "zep/range_markers.h"
#include "zep/buffer.h"

namespace Zep
{

RangeMarker::RangeMarker(ZepBuffer& buffer)
    : m_buffer(buffer)
{
}

bool RangeMarker::ContainsLocation(GlyphIterator loc) const
{
    return m_range.ContainsLocation(loc.Index());
}

bool RangeMarker::IntersectsRange(const ByteRange& i) const
{
    return i.first < m_range.second && i.second > m_range.first;
}

ThemeColor RangeMarker::GetBackgroundColor(const GlyphIterator& itr) const
{
    ZEP_UNUSED(itr);
    return m_backgroundColor;
}

ThemeColor RangeMarker::GetTextColor(const GlyphIterator& itr) const
{
    ZEP_UNUSED(itr);
    return m_textColor;
}

ThemeColor RangeMarker::GetHighlightColor(const GlyphIterator& itr) const
{
    ZEP_UNUSED(itr);
    return m_highlightColor;
}

float RangeMarker::GetAlpha(GlyphIterator) const
{
    return alpha;
}

void RangeMarker::SetBackgroundColor(ThemeColor color)
{
    m_backgroundColor = color;
}

void RangeMarker::SetTextColor(ThemeColor color)
{
    m_textColor = color;
}

void RangeMarker::SetHighlightColor(ThemeColor color)
{
    m_highlightColor = color;
}

void RangeMarker::SetColors(ThemeColor back, ThemeColor text, ThemeColor highlight)
{
    m_backgroundColor = back;
    m_textColor = text;
    m_highlightColor = highlight;
}

void RangeMarker::SetAlpha(float a)
{
    alpha = a;
}
    
void RangeMarker::SetRange(ByteRange range)
{
    auto spMarker = shared_from_this();
    m_buffer.ClearRangeMarker(spMarker);
    
    m_range = range;
    m_buffer.AddRangeMarker(spMarker);
}

const ByteRange& RangeMarker::GetRange() const
{
    return m_range;
}

}; // namespace Zep
