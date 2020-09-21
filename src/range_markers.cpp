#include "zep/editor.h"
#include "zep/range_markers.h"

namespace Zep
{

bool RangeMarker::ContainsLocation(GlyphIterator loc) const
{
    return range.ContainsLocation(loc.Index());
}

bool RangeMarker::IntersectsRange(const ByteRange& i) const
{
    return i.first < range.second && i.second > range.first;
}

ThemeColor RangeMarker::GetBackgroundColor(const GlyphIterator& itr) const
{
    ZEP_UNUSED(itr);
    return backgroundColor;
}

ThemeColor RangeMarker::GetTextColor(const GlyphIterator& itr) const
{
    ZEP_UNUSED(itr);
    return textColor;
}

ThemeColor RangeMarker::GetHighlightColor(const GlyphIterator& itr) const
{
    ZEP_UNUSED(itr);
    return highlightColor;
}

float RangeMarker::GetAlpha(GlyphIterator) const
{
    return alpha;
}

void RangeMarker::SetBackgroundColor(ThemeColor color)
{
    backgroundColor = color;
}

void RangeMarker::SetTextColor(ThemeColor color)
{
    textColor = color;
}

void RangeMarker::SetHighlightColor(ThemeColor color)
{
    highlightColor = color;
}

void RangeMarker::SetColors(ThemeColor back, ThemeColor text, ThemeColor highlight)
{
    backgroundColor = back;
    textColor = text;
    highlightColor = highlight;
}

void RangeMarker::SetAlpha(float a)
{
    alpha = a;
}

}; // namespace Zep
