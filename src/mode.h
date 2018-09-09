#pragma once

#include <stack>
#include "buffer.h"
#include "display.h"

namespace Zep
{

class ZepEditor;
class ZepCommand;

struct ExtKeys
{
    enum Key
    {
        RETURN,
        ESCAPE,
        BACKSPACE,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        TAB,
        DEL,
        HOME,
        END,
        PAGEDOWN,
        PAGEUP
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
};

enum class EditorMode
{
    None,
    Normal,
    Insert,
    Visual,
    Command
};

class ZepMode : public ZepComponent
{
public:
    ZepMode(ZepEditor& editor);
    virtual ~ZepMode();

    // Keys handled by modes
    virtual void AddCommandText(std::string strText);
    virtual void AddKeyPress(uint32_t key, uint32_t modifierKeys = ModifierKey::None) = 0;
    virtual const char* Name() const = 0;
    virtual void Enable() = 0;
    virtual void Notify(std::shared_ptr<ZepMessage> message) override {}
    virtual void AddCommand(std::shared_ptr<ZepCommand> spCmd);
    virtual void SetCurrentWindow(ZepWindow* pWindow);
    virtual void UpdateVisualSelection();

    virtual void Undo();
    virtual void Redo();
protected:
    std::stack<std::shared_ptr<ZepCommand>> m_undoStack;
    std::stack<std::shared_ptr<ZepCommand>> m_redoStack;
    ZepWindow* m_pCurrentWindow = nullptr;
    EditorMode m_currentMode;
    bool m_lineWise = false;
    BufferLocation m_insertBegin;
    BufferLocation m_visualBegin;
    BufferLocation m_visualEnd;
};

} // Zep
