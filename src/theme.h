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
    Mode
};

class Theme
{
public:
    Theme();
    static Theme& Instance();
    virtual NVec4f GetSyntaxColor(uint32_t syntax) const;
    virtual NVec4f GetColor(ThemeColor themeColor) const;
    virtual NVec4f GetComplement(const NVec4f& col) const;
    virtual NVec4f GetUniqueColor(uint32_t id) const;
private:
    std::vector<NVec4f> m_uniqueColors;
};

}
