#pragma once

#include "zep/mode_vim.h"
#include "zep/keymap.h"

namespace Zep
{

class ZepMode_Orca : public ZepMode_Vim
{
public:
    ZepMode_Orca(ZepEditor& editor);
    ~ZepMode_Orca();

    static const char* StaticName()
    {
        return "Orca";
    }

    static void Register(ZepEditor& editor);

    // Zep Mode
    virtual void Begin(ZepWindow* pWindow) override;
    virtual const char* Name() const override { return StaticName(); }
    virtual void PreDisplay(ZepWindow& win) override;

    virtual void SetupKeyMaps() override;

    virtual uint32_t ModifyWindowFlags(uint32_t flags) override;

private:
    ZepWindow* m_pCurrentWindow;
};

} // namespace Zep
