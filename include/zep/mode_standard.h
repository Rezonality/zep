#pragma once

#include "mode.h"

namespace Zep
{

class ZepMode_Standard : public ZepMode
{
public:
    ZepMode_Standard(ZepEditor& editor);
    ~ZepMode_Standard();

    virtual void Begin(ZepWindow* pWindow) override;

    static const char* StaticName()
    {
        return "Standard";
    }
    virtual const char* Name() const override
    {
        return StaticName();
    }

private:
    void Init();

private:
    std::string keyCache;
};

} // namespace Zep
