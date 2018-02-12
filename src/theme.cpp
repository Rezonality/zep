#include "editor.h"
#include "theme.h"
#include "syntax.h"

namespace Zep
{

Theme& Theme::Instance()
{
    static Theme instance;
    return instance;
}

// Return color based on theme
uint32_t Theme::GetColor(uint32_t type) const
{
    switch (type)
    {
    default:
    case SyntaxType::Normal:
        return 0xFFFFFFFF;
    case SyntaxType::Comment:
        return 0xFF00FF11;
    case SyntaxType::Keyword:
        return 0xFFFFFF11;
    case SyntaxType::Integer:
        return 0xFF11FFFF;
    case SyntaxType::Whitespace:
        return 0xFF223322;
    }
}

}