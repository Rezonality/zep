#include "theme.h"
#include "editor.h"
#include "syntax.h"

namespace Zep
{

Theme::Theme()
{
    double golden_ratio_conjugate = 0.618033988749895;
    double h = .4f;
    for(int i = 0; i < 10; i++)
    {
        h += golden_ratio_conjugate;
        h = std::fmod(h, 1.0);
        m_uniqueColors.emplace_back(HSVToRGB(float(h) * 360.0f, 0.7f, 255.0f));
    }
}

Theme& Theme::Instance()
{
    static Theme instance;
    return instance;
}

NVec4f Theme::GetUniqueColor(uint32_t index) const
{
    return m_uniqueColors[index % m_uniqueColors.size()];
}

NVec4f Theme::GetColor(ThemeColor themeColor) const
{
    switch (themeColor)
    {
    case ThemeColor::Text:
        return NVec4f(1.0f);
    case ThemeColor::HiddenText:
        return NVec4f(.9f, .1f, .1f, 1.0f);
    case ThemeColor::TabBorder:
        return NVec4f(.55f, .55f, .55f, 1.0f);
    case ThemeColor::Tab:
        return NVec4f(.4f, .4f, .4f, .55f);
    case ThemeColor::TabActive:
        return NVec4f(.55f, .55f, .55f, 1.0f);
    case ThemeColor::LineNumberBackground:
        return NVec4(.13f, .13f, .13f, 1.0f);
    case ThemeColor::LineNumber:
        return NVec4(.13f, 1.0f, .13f, 1.0f);
    case ThemeColor::LineNumberActive:
        return NVec4(.13f, 1.0f, .13f, 1.0f);
    case ThemeColor::CursorNormal:
        return NVec4(130.0f / 255.0f, 140.0f / 255.0f, 230.0f / 255.0f, 1.0f);
    case ThemeColor::CursorInsert:
        return NVec4(1.0f, 1.0f, 1.0f, .9f);
    case ThemeColor::CursorLineBackground:
        return NVec4(.15f, .15f, .15f, 1.0f);
    case ThemeColor::AirlineBackground:
        return NVec4(.12f, .12f, .12f, 1.0f);
    case ThemeColor::Light:
        return NVec4(1.0f);
    case ThemeColor::Dark:
        return NVec4(0.0f, 0.0f, 0.0f, 1.0f);
    case ThemeColor::VisualSelectBackground:
        return NVec4(.47f, 0.30f, 0.25f, 1.0f);
    case ThemeColor::Mode:
        return NVec4(.2f, 0.8f, 0.2f, 1.0f);
    default:
        return NVec4f(1.0f);
        break;
    }
}

// Return color based on theme
NVec4f Theme::GetSyntaxColor(uint32_t type) const
{
    switch (type)
    {
    default:
    case SyntaxType::Normal:
        return Theme::Instance().GetColor(ThemeColor::Text);
    case SyntaxType::Parenthesis:
        return Theme::Instance().GetColor(ThemeColor::Text);
    case SyntaxType::Comment:
        return NVec4f(0.0f, 1.0f, .1f, 1.0f);
    case SyntaxType::Keyword:
        return NVec4f(0.1f, 1.0f, 1.0f, 1.0f);
    case SyntaxType::Integer:
        return NVec4f(.1f, 1.0f, 1.0f, 1.0f);
    case SyntaxType::Whitespace:
        return NVec4f(.15f, .2f, .15f, 1.0f);
    }
}
NVec4f Theme::GetComplement(const NVec4f& col) const
{
    auto lum = Luminosity(col);
    if (lum > 0.5f)
        return GetColor(ThemeColor::Dark);
    return GetColor(ThemeColor::Light);
}


} // namespace Zep