#pragma once

#include <stack>

#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/window.h"
#include "zep/commands.h"
#include "zep/keymap.h"

namespace Zep
{

class ZepEditor;

// NOTE: These are input keys mapped to Zep's internal keymapping; they live below 'space'/32
// Key mapping needs a rethink for international keyboards.  But for modes, this is the remapped key definitions for anything that isn't
// basic ascii symbol.  ASCII 0-31 are mostly ununsed these days anyway.
struct ExtKeys
{
    enum Key
    {
        RETURN = 0, // NOTE: Do not change this value
        ESCAPE = 1,
        BACKSPACE = 2,
        LEFT = 3,
        RIGHT = 4,
        UP = 5,
        DOWN = 6,
        TAB = 7,
        DEL = 8,
        HOME = 9,
        END = 10,
        PAGEDOWN = 11,
        PAGEUP = 12,
        F1 = 13,
        F2 = 14,
        F3 = 15,
        F4 = 16,
        F5 = 17,
        F6 = 18,
        F7 = 19,
        F8 = 20,
        F9 = 21,
        F10 = 22,
        F11 = 23,
        F12 = 24,

        // Note: No higher than 31
        LAST = 31,
        NONE = 32
    };
};

struct ModifierKey
{
    enum Key
    {
        None = (0),
        Ctrl = (1 << 0),
        Alt = (1 << 1),
        Shift = (1 << 2)
    };
}; // ModifierKey

enum class EditorMode
{
    None,
    Normal,
    Insert,
    Visual,
    Ex
};

enum class CommandOperation
{
    None,
    Delete,
    DeleteLines,
    Insert,
    Copy,
    CopyLines,
    Replace,
    Paste
};

namespace ModeFlags
{
enum
{
    None = (0),
    InsertModeGroupUndo = (1 << 0)
};
}

namespace CommandResultFlags
{
enum
{
    None = 0,
    HandledCount = (1 << 2), // Command implements the count, no need to recall it.
    BeginUndoGroup = (1 << 4)
};
} // CommandResultFlags

struct CommandResult
{
    uint32_t flags = CommandResultFlags::None;
    EditorMode modeSwitch = EditorMode::None;
    std::shared_ptr<ZepCommand> spCommand;
};

class CommandContext
{
public:
    CommandContext(const std::string& commandIn, ZepMode& md, EditorMode editorMode);

    void GetCommandRegisters();
    void UpdateRegisters();

    ZepMode& owner;

    std::string fullCommand;
    KeyMapResult keymap;

    ReplaceRangeMode replaceRangeMode = ReplaceRangeMode::Fill;
    ByteIndex beginRange{-1};
    ByteIndex endRange{-1};
    ZepBuffer& buffer;

    // Cursor State
    ByteIndex bufferCursor{-1};
    ByteIndex cursorAfterOverride{-1};

    // Register state
    std::stack<char> registers;
    Register tempReg;
    const Register* pRegister = nullptr;

    // Input State
    EditorMode currentMode = EditorMode::None;

    // Output result
    CommandResult commandResult;
    CommandOperation op = CommandOperation::None;

    bool foundCommand = false;
};

class ZepMode : public ZepComponent
{
public:
    ZepMode(ZepEditor& editor);
    virtual ~ZepMode();

    virtual void Init() {};

    virtual void AddKeyPress(uint32_t key, uint32_t modifierKeys = ModifierKey::None);
    virtual const char* Name() const = 0;
    virtual void Begin() = 0;
    virtual void Notify(std::shared_ptr<ZepMessage> message) override {}
    virtual uint32_t ModifyWindowFlags(uint32_t windowFlags) { return windowFlags; }

    // Do the actual input handling
    virtual void HandleMappedInput(const std::string& input);

    // Keys handled by modes
    virtual void AddCommandText(std::string strText);
    virtual void AddCommand(std::shared_ptr<ZepCommand> spCmd);
    virtual EditorMode GetEditorMode() const;
    virtual void SetEditorMode(EditorMode currentMode);

    // About to display this window, which is associated with this mode
    virtual void PreDisplay(ZepWindow&){};

    // Called when we begin editing in this mode

    virtual void Undo();
    virtual void Redo();

    virtual ZepWindow* GetCurrentWindow() const;

    virtual NVec2i GetNormalizedVisualRange() const;

    virtual const std::string& GetLastCommand() const;
    virtual bool GetCommand(CommandContext& context);
    virtual void ResetCommand();

    virtual bool GetOperationRange(const std::string& op, EditorMode currentMode, ByteIndex& beginRange, ByteIndex& endRange) const;

    virtual void UpdateVisualSelection();

    const KeyMap& GetKeyMappings(EditorMode mode) const;

    void AddGlobalKeyMaps();
    void AddNavigationKeyMaps(bool allowInVisualMode = true);
    void AddSearchKeyMaps();
    void AddKeyMapWithCountRegisters(const std::vector<KeyMap*>& maps, const std::vector<std::string>& commands, const StringId& id);


protected:
    virtual void SwitchMode(EditorMode currentMode);
    virtual void ClampCursorForMode();
    virtual bool HandleExCommand(std::string strCommand);
    virtual std::string ConvertInputToMapString(uint32_t key, uint32_t modifierKeys);

protected:
    std::stack<std::shared_ptr<ZepCommand>> m_undoStack;
    std::stack<std::shared_ptr<ZepCommand>> m_redoStack;
    EditorMode m_currentMode = EditorMode::Normal;
    bool m_lineWise = false;
    ByteIndex m_visualBegin = 0;
    ByteIndex m_visualEnd = 0;
    std::string m_dotCommand;

    // Keyboard mappings
    KeyMap m_normalMap;
    KeyMap m_visualMap;
    KeyMap m_insertMap;
    
    SearchDirection m_lastFindDirection = SearchDirection::Forward;
    SearchDirection m_lastSearchDirection = SearchDirection::Forward;

    std::string m_currentCommand;
    std::string m_lastInsertString;
    std::string m_lastFind;

    ByteIndex m_exCommandStartLocation = 0;
    bool m_pendingEscape = false;

    CursorType m_visualCursorType = CursorType::Visual;
    uint32_t m_modeFlags = ModeFlags::None;
    uint32_t m_lastKey = 0;
};

} // namespace Zep
