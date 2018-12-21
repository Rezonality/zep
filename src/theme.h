#pragma once

#include "editor.h"
#include "syntax.h"

namespace Zep
{

enum class ThemeColor
{
    TabBorder,
    HiddenText,
    Text,
    Background,
    Tab,
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
    Integer,
    Comment,
    Whitespace,
    HiddenChar,
    Parenthesis
};

class ZepTheme
{
public:
    ZepTheme();
    virtual NVec4f GetColor(ThemeColor themeColor) const;
    virtual NVec4f GetComplement(const NVec4f& col) const;
    virtual NVec4f GetUniqueColor(uint32_t id) const;

    void SetDarkTheme();
    void SetLightTheme();
private:
    std::vector<NVec4f> m_uniqueColors;
    std::map<ThemeColor, NVec4f> m_colors;
};

}
