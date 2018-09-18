#pragma once

#include "mode.h"

namespace Zep
{

class ZepMode_Standard : public ZepMode
{
public:
    ZepMode_Standard(ZepEditor& editor);
    ~ZepMode_Standard();

    virtual void AddKeyPress(uint32_t key, uint32_t modifiers = 0) override;
    virtual void Begin() override;

    virtual const char* Name() const override { return "Standard"; }
private:
    virtual void SwitchMode(EditorMode mode);
};

} // Zep
