#pragma once

#include "mode.h"
#include "zep/keymap.h"

class Timer;

namespace Zep
{

struct SpanInfo;

enum class VimMotion
{
    LineBegin,
    LineEnd,
    NonWhiteSpaceBegin,
    NonWhiteSpaceEnd
};

class ZepMode_Vim : public ZepMode
{
public:
    ZepMode_Vim(ZepEditor& editor);
    ~ZepMode_Vim();

    static const char* StaticName()
    {
        return "Vim";
    }

    // Zep Mode
    virtual void Begin(ZepWindow* pWindow) override;
    virtual const char* Name() const override { return StaticName(); }
    virtual void PreDisplay(ZepWindow& win) override;
    virtual void SetupKeyMaps();
    virtual void AddOverStrikeMaps();
    virtual void AddCopyMaps();
    virtual void AddPasteMaps();
    virtual void Init() override;

private:
    void HandleInsert(uint32_t key);
};

} // namespace Zep
