#pragma once

#include "mode.h"
#include <future>
#include <memory>
#include <regex>

namespace Zep
{

struct ZepRepl;

class ZepMode_Repl : public ZepMode
{
public:
    ZepMode_Repl(ZepEditor& editor, ZepWindow& launchWindow, ZepWindow& replWindow);
    ~ZepMode_Repl();

    virtual void AddKeyPress(uint32_t key, uint32_t modifiers = 0) override;
    virtual void Begin() override;
    virtual void Notify(std::shared_ptr<ZepMessage> message) override;
    
    static const char* StaticName()
    {
        return "REPL";
    }
    virtual const char* Name() const override
    {
        return StaticName();
    }

private:
    void Close();

private:
    void BeginInput();
    BufferLocation m_startLocation = BufferLocation{ 0 };
    ZepWindow& m_launchWindow;
    ZepWindow& m_replWindow;
    ZepRepl* m_pRepl = nullptr;
};

} // namespace Zep
