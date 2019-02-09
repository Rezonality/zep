#pragma once

#include "mode.h"

namespace Zep
{

class ZepMode_Search : public ZepMode
{
public:
    ZepMode_Search(ZepEditor& editor) : ZepMode(editor) {  }
    ~ZepMode_Search() {};

    virtual void AddKeyPress(uint32_t key, uint32_t modifiers = 0) override
    {
        (void)key;
        (void)modifiers;
    };

    virtual void Begin() override
    {
    }

    static const char* StaticName()
    {
        return "Search";
    }
    virtual const char* Name() const override
    {
        return StaticName();
    }
};

} // namespace Zep
