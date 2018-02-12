#pragma once

namespace Zep
{

class Theme
{
public:
    static Theme& Instance();
    virtual uint32_t GetColor(uint32_t syntax) const;
};

}
