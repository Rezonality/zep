#pragma once

#include "mcommon/math/math.h"

namespace Zep
{

enum class ThemeColor
{
    TabBorder,
    HiddenText,
    Text,
    Background,
    TabInactive,
    TabActive,
    LineNumberBackground,
    LineNumber,
    LineNumberActive,
    CursorNormal,
    CursorInsert,
    Light,
    Dark,
    VisualSelectBackground,
    CursorLineBackground,
    AirlineBackground,
    Mode,
    Normal,
    Keyword,
    Identifier,
    Number,
    String,
    Comment,
    Whitespace,
    HiddenChar,
    Parenthesis,
    Error,
    Warning,
    Info,
    WidgetBackground,
    WidgetActive,
    WidgetInactive
};

enum class ThemeType
{
    Dark,
    Light
};

class ZepTheme
{
public:
    ZepTheme();
    virtual ~ZepTheme() {}

    virtual NVec4f GetColor(ThemeColor themeColor) const;
    virtual NVec4f GetComplement(const NVec4f& col, const NVec4f& adjust = NVec4f(0.0f)) const;
    virtual NVec4f GetUniqueColor(uint32_t id) const;

    void SetThemeType(ThemeType type);
    ThemeType GetThemeType() const;

private:
    void SetDarkTheme();
    void SetLightTheme();

private:
    std::vector<NVec4f> m_uniqueColors;
    std::map<ThemeColor, NVec4f> m_colors;
    ThemeType m_currentTheme = ThemeType::Dark;
};

} // namespace Zep
