#pragma once

#include <stack>
#include "buffer.h"
#include "display.h"

namespace Zep
{

class ZepEditor;
class ZepCommand;
class ZepWindow;

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
    Visual
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
    virtual void Notify(std::shared_ptr<ZepMessage> message) override
    {
    }
    virtual void AddCommand(std::shared_ptr<ZepCommand> spCmd);
    virtual EditorMode GetEditorMode() const;

    // Called when we begin editing in this mode
    virtual void Begin() = 0;

    virtual void Undo();
    virtual void Redo();

    virtual ZepWindow* GetCurrentWindow() const;
    virtual void PreDisplay(){};

    virtual NVec2i GetVisualRange() const;

protected:
    std::stack<std::shared_ptr<ZepCommand>> m_undoStack;
    std::stack<std::shared_ptr<ZepCommand>> m_redoStack;
    EditorMode m_currentMode = EditorMode::Normal;
    bool m_lineWise = false;
    BufferLocation m_insertBegin = 0;
    BufferLocation m_visualBegin = 0;
    BufferLocation m_visualEnd = 0;
};

} // namespace Zep
