#include "zep/display.h"

#include "zep/mcommon/logger.h"
#include "zep/mcommon/string/stringutils.h"

#define UTF_CPP_CPLUSPLUS 201103L // C++ 11 or later
#include "zep/mcommon/utf8/unchecked.h"

// A 'window' is like a vim window; i.e. a region inside a tab
namespace Zep
{

void ZepDisplay::InvalidateCharCache()
{
    m_charCacheDirty = true;
}

void ZepDisplay::BuildCharCache()
{
    const char chA = 'A';
    m_defaultCharSize = GetGapBufferSize((const uint8_t*)&chA, (const uint8_t*)&chA + 1);
    for (int i = 0; i < 127; i++)
    {
        uint8_t ch = (uint8_t)i;
        m_charCacheASCII[i] = GetGapBufferSize(&ch, &ch + 1);
    }
    m_charCacheDirty = false;
}

const NVec2f& ZepDisplay::GetDefaultCharSize()
{
    if (m_charCacheDirty)
    {
        BuildCharCache();
    }
    return m_defaultCharSize;
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

NVec2f ZepDisplay::GetCharSize(const uint8_t* pCh)
{
    if (m_charCacheDirty)
    {
        BuildCharCache();
    }

    if (utf8_codepoint_length(*pCh) == 1)
    {
        return m_charCacheASCII[*pCh];
    }
 
    auto ch32 = utf8::unchecked::next(pCh);

    auto itr = m_charCache.find((uint32_t)ch32);
    if (itr != m_charCache.end())
    {
        return itr->second;
    }
     
    auto sz = GetGapBufferSize(pCh, pCh + utf8_codepoint_length(*pCh));
    m_charCache[(uint32_t)ch32] = sz;

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
