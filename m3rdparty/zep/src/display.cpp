#include "display.h"

#include "mcommon/logger.h"
#include "mcommon/string/stringutils.h"

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
    m_defaultCharSize = GetTextSize((const utf8*)&chA, (const utf8*)&chA + 1);
    for (int i = 0; i < 256; i++)
    {
        utf8 ch = (utf8)i;
        m_charCache[i] = GetTextSize(&ch, &ch + 1);
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

NVec2f ZepDisplay::GetCharSize(const utf8* pCh)
{
    if (m_charCacheDirty)
    {
        BuildCharCache();
    }
    return m_charCache[*pCh];
}

}
