#pragma once

#include <mutex>

#include "zep/mode_vim.h"
#include "zep/keymap.h"
#include "zep/syntax.h"

namespace Zep
{

class Orca;
class ZepMode_Orca : public ZepMode_Vim
{
public:
    ZepMode_Orca(ZepEditor& editor);
    ~ZepMode_Orca();

    static const char* StaticName()
    {
        return "Orca";
    }

    static std::shared_ptr<ZepMode_Orca> Register(ZepEditor& editor);

    virtual void Init() override;

    // Zep Mode
    virtual void Begin(ZepWindow* pWindow) override;
    virtual const char* Name() const override { return StaticName(); }
    virtual void PreDisplay(ZepWindow& win) override;

    virtual void SetupKeyMaps() override;

    virtual uint32_t ModifyWindowFlags(uint32_t flags) override;
    
    virtual std::vector<Airline> GetAirlines(ZepWindow& window) const override;

    virtual bool HandleSpecialCommand(CommandContext& context) override;

    virtual Orca* GetCurrentOrca() const;

    virtual void Notify(std::shared_ptr<ZepMessage> spMsg) override;

    virtual bool HandleIgnoredInput(CommandContext& context) override;

    const std::map<ZepBuffer*, std::shared_ptr<Orca>>& GetOrcaBuffers() const { return m_mapOrca; }

private:
    ZepWindow* m_pCurrentWindow;
    std::map<ZepBuffer*, std::shared_ptr<Orca>> m_mapOrca;
    uint32_t m_bufferWidth = 0;
    uint32_t m_bufferHeight = 0;
};

} // namespace Zep
