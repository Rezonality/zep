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
    virtual void Begin() override;
    virtual const char* Name() const override { return StaticName(); }
    virtual void PreDisplay() override;

private:
    void HandleInsert(uint32_t key);
    void Init();

    timer m_insertEscapeTimer;
};

inline std::string MakeCommandRegex(const std::string& command)
{
    return std::string(R"((?:(\d)|(<\S>*)|("\w?)*)()") + command + ")";
}

} // namespace Zep
