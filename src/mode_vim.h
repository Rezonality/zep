#pragma once

#include "mode.h"

class Timer;

namespace Zep
{

struct LineInfo;

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
    HandledCount = (1 << 2), // Command implements the count, no need to recall it.
    NeedMoreChars
};
}

struct VimSettings
{
    bool ShowNormalModeKeyStrokes = false;
};

struct CommandResult
{
    uint32_t flags = CommandResultFlags::None;
    EditorMode modeSwitch = EditorMode::None;
    std::shared_ptr<ZepCommand> spCommand;
};

enum class CommandOperation
{
    None,
    Delete,
    DeleteLines,
    Insert,
    Copy,
    CopyLines
};

struct CommandContext
{
    CommandContext(const std::string& commandIn, ZepMode_Vim& md, uint32_t lastK, uint32_t modifierK, EditorMode editorMode);
   
    // Parse the command into:
    // [count1] opA [count2] opB
    // And generate (count1 * count2), opAopB
    void GetCommandAndCount();
    void GetCommandRegisters();
    void UpdateRegisters();

    ZepMode_Vim& owner;

    std::string commandText;
    std::string commandWithoutCount;
    std::string command;
    const LineInfo* pLineInfo = nullptr;
    long displayLineCount = 0;
    BufferLocation beginRange{ -1 };
    BufferLocation endRange{ -1 };
    ZepBuffer& buffer;

    // Cursor State
    BufferLocation bufferLocation{ -1 };
    BufferLocation cursorAfter{ -1 };

    // Register state
    std::stack<char> registers;
    Register tempReg;
    const Register* pRegister = nullptr;

    // Input State
    uint32_t lastKey = 0;
    uint32_t modifierKeys = 0;
    EditorMode mode = EditorMode::None;
    int count = 1;

    // Output result
    CommandResult commandResult;
    CommandOperation op = CommandOperation::None;
};

class ZepMode_Vim : public ZepMode
{
public:
    ZepMode_Vim(ZepEditor& editor);
    ~ZepMode_Vim();

    virtual void AddKeyPress(uint32_t key, uint32_t modifiers = 0) override;
    virtual void Begin() override;

    virtual const char* Name() const override { return "Vim"; }

    const std::string& GetLastCommand() const { return m_lastCommand; }
    const int GetLastCount() const { return m_lastCount; }

    virtual void PreDisplay() override;
private:
    void HandleInsert(uint32_t key);
    bool GetBlockOpRange(const std::string& op, EditorMode mode, BufferLocation& beginRange, BufferLocation& endRange, BufferLocation& cursorAfter) const;
    void SwitchMode(EditorMode mode);
    void ResetCommand();
    void Init();
    bool GetCommand(CommandContext& context);

    std::string m_currentCommand;
    std::string m_lastCommand;
    int m_lastCount;
    std::string m_lastInsertString;

    bool m_pendingEscape = false;
    std::shared_ptr<Timer> m_spInsertEscapeTimer;
    VimSettings m_settings;
};

} // Zep
