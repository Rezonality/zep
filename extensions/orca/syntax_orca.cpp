#include "zep/editor.h"
#include "zep/theme.h"

#include "zep/mcommon/logger.h"
#include "zep/mcommon/string/stringutils.h"

#include "mode_orca.h"
#include "syntax_orca.h"

#include <string>
#include <vector>

namespace Zep
{

ZepSyntax_Orca::ZepSyntax_Orca(ZepBuffer& buffer,
    const std::unordered_set<std::string>& keywords,
    const std::unordered_set<std::string>& identifiers,
    uint32_t flags)
    : ZepSyntax(buffer, keywords, identifiers, flags)
{
    // Don't need default
    m_adornments.clear();
}

SyntaxResult ZepSyntax_Orca::GetSyntaxAt(const GlyphIterator& itr) const
{
    auto def = ZepSyntax::GetSyntaxAt(itr);
    if (m_syntax.size() <= itr.Index())
    {
        SyntaxResult res;
        res.foreground = ThemeColor::Text;
        res.foreground = ThemeColor::None;
        return res;
    }

    // Accomodate potential background effects from the base class (such as flash)
    // Looks cool (:ZTestFlash) but is it useful??
    auto ret = m_syntax[itr.Index()];
    if (ret.background == ThemeColor::None && def.background != ThemeColor::None &&
        def.background != ThemeColor::Background)
    {
        ret.background = def.background;
        ret.customBackgroundColor = def.customBackgroundColor;
    }

    return ret;
}

void ZepSyntax_Orca::UpdateSyntax()
{
    auto& buffer = m_buffer.GetWorkingBuffer();

    // We don't do anything in orca mode, we get dynamically because Orca tells us the colors
    m_targetChar = long(0);
    m_processedChar = long(buffer.size() - 1);
}

void ZepSyntax_Orca::UpdateSyntax(std::vector<SyntaxResult>& syntax)
{
    m_syntax = syntax;
}

} // namespace Zep
