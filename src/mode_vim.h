#pragma once

#include "mode.h"

class Timer;

namespace Zep
{

enum class VimMotion
{
    LineBegin,
    LineEnd,
    NonWhiteSpaceBegin,
    NonWhiteSpaceEnd
};

namespace CommandResultFlags
{
enum
{
    None = 0,
    HandledCount = (1 << 2) // Command implements the count, no need to recall it.
};
}

struct CommandResult
{
    uint32_t flags = CommandResultFlags::None;
    EditorMode modeSwitch = EditorMode::None;
    std::shared_ptr<ZepCommand> spCommand;
};

class ZepMode_Vim : public ZepMode
{
public:
    ZepMode_Vim(ZepEditor& editor);
    ~ZepMode_Vim();

    virtual void AddKeyPress(uint32_t key, uint32_t modifiers = 0) override;
    virtual void Enable() override;
    virtual void SetCurrentWindow(ZepWindow* pWindow) override;

    virtual const char* Name() const override { return "Vim"; }
private:
    void HandleInsert(uint32_t key);
    std::string GetCommandAndCount(std::string strCommand, int& count);
    bool GetBlockOpRange(const std::string& op, EditorMode mode, BufferLocation& beginRange, BufferLocation& endRange, BufferLocation& cursorAfter) const;
    void SwitchMode(EditorMode mode);
    void ResetCommand();
    bool GetCommand(std::string strCommand, uint32_t lastKey, uint32_t modifiers, EditorMode mode, int count, CommandResult& commandResult);
    void Init();

    std::string m_currentCommand;
    std::string m_lastCommand;
    int m_lastCount;
    std::string m_lastInsertString;

    bool m_pendingEscape = false;
    std::shared_ptr<Timer> m_spInsertEscapeTimer;
};

} // Zep
