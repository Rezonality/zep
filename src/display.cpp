#include "zep/display.h"

#include "zep/mcommon/logger.h"
#include "zep/mcommon/string/stringutils.h"

#define UTF_CPP_CPLUSPLUS 201103L // C++ 11 or later
#include "zep/mcommon/utf8/unchecked.h"

// A 'window' is like a vim window; i.e. a region inside a tab
namespace Zep
{

void ZepDisplay::InvalidateCharCache(ZepFontType type)
{
    GetFontCache(type).charCacheDirty = true;
}

ZepDisplay::FontTypeCache& ZepDisplay::GetFontCache(ZepFontType type)
{
    return m_fontCache[int(type)];
}

void ZepDisplay::BuildCharCache(ZepFontType type)
{
    auto& fontCache = GetFontCache(type);

    const char chA = 'A';
    fontCache.defaultCharSize = GetTextSize(type, (const uint8_t*)&chA, (const uint8_t*)&chA + 1);
    for (int i = 0; i < 256; i++)
    {
        uint8_t ch = (uint8_t)i;
        fontCache.charCacheASCII[i] = GetTextSize(type, &ch, &ch + 1);
    }
    fontCache.charCacheDirty = false;
    
    fontCache.dotSize = fontCache.defaultCharSize / 8.0f;
    fontCache.dotSize.x = std::min(fontCache.dotSize.x, fontCache.dotSize.y);
    fontCache.dotSize.y = std::min(fontCache.dotSize.x, fontCache.dotSize.y);
    fontCache.dotSize.x = std::max(1.0f, fontCache.dotSize.x);
    fontCache.dotSize.y = std::max(1.0f, fontCache.dotSize.y);
}

const NVec2f& ZepDisplay::GetDefaultCharSize(ZepFontType type)
{
    auto& fontCache = GetFontCache(type);
    if (fontCache.charCacheDirty)
    {
        BuildCharCache(type);
    }
    
    return fontCache.defaultCharSize;
}

const NVec2f& ZepDisplay::GetDotSize(ZepFontType type)
{
    return GetFontCache(type).dotSize;
}

uint32_t ZepDisplay::GetCodePointCount(const uint8_t* pCh, const uint8_t* pEnd) const
{
    uint32_t count = 0;
    while (pCh < pEnd)
    {
        pCh += utf8_codepoint_length(*pCh);
        count++;
    }
    return count;
}

NVec2f ZepDisplay::GetCharSize(ZepFontType type, const uint8_t* pCh)
{
    auto& fontCache = GetFontCache(type);
    if (fontCache.charCacheDirty)
    {
        BuildCharCache(type);
    }

    if (utf8_codepoint_length(*pCh) == 1)
    {
        return fontCache.charCacheASCII[*pCh];
    }
 
    auto ch32 = utf8::unchecked::next(pCh);

    auto itr = fontCache.charCache.find((uint32_t)ch32);
    if (itr != fontCache.charCache.end())
    {
        return itr->second;
    }
     
    auto sz = GetTextSize(type, pCh, pCh + utf8_codepoint_length(*pCh));
    fontCache.charCache[(uint32_t)ch32] = sz;

    return sz;
}
    
void ZepDisplay::DrawRect(const NRectf& rc, const NVec4f& col) const
{
    DrawLine(rc.topLeftPx, rc.BottomLeft(), col);
    DrawLine(rc.topLeftPx, rc.TopRight(), col);
    DrawLine(rc.TopRight(), rc.bottomRightPx, col);
    DrawLine(rc.BottomLeft(), rc.bottomRightPx, col);
}

}
