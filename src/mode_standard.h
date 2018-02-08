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
    virtual void Enable() override;
    virtual void SetCurrentWindow(ZepWindow* pWindow) override;

    virtual const char* Name() const override { return "Modeless"; }
private:
    virtual void SwitchMode(EditorMode mode);
};

} // Zep
