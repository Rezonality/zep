#include "mode_standard.h"
#include "commands.h"
#include "mcommon/string/stringutils.h"
#include "window.h"

// Note:
// This is a version of the buffer that behaves like notepad.
// It is basic, but can easily be extended

// STANDARD:
//
// DONE:
// -----
// CTRLZ/Y Undo Redo
// Insert
// Delete/Backspace
// TAB
// Arrows - up,down, left, right
// Home (+Ctrl) move top/startline
// End (+Ctrol) move bottom/endline
// Shift == Select
// control+Shift == select word
// CTRL - CVX (copy paste, cut) + Delete Selection

namespace Zep
{

ZepMode_Standard::ZepMode_Standard(ZepEditor& editor)
    : ZepMode(editor)
{
}

ZepMode_Standard::~ZepMode_Standard()
{
}

void ZepMode_Standard::Begin()
{
    SwitchMode(EditorMode::Insert);
}

void ZepMode_Standard::SwitchMode(EditorMode mode)
{
    assert(mode == EditorMode::Insert || mode == EditorMode::Visual);
    m_currentMode = mode;
    if (GetCurrentWindow())
    {
        GetCurrentWindow()->SetCursorMode(mode == EditorMode::Insert ? CursorMode::Insert : CursorMode::Visual);
    }
    if (mode == EditorMode::Insert)
    {
        m_visualBegin = m_visualEnd = 0;
    }
}

void ZepMode_Standard::AddKeyPress(uint32_t key, uint32_t modifierKeys)
{
    std::string ch((char*)&key);

    bool copyRegion = false;
    bool pasteText = false;
    bool lineWise = false;
    auto& buffer = GetCurrentWindow()->GetBuffer();
    BufferLocation bufferCursor = GetCurrentWindow()->GetBufferCursor();
    BufferLocation startOffset = bufferCursor;
    BufferLocation endOffset = buffer.LocationFromOffsetByChars(bufferCursor, long(ch.length()));
    BufferLocation cursorAfter = endOffset;

    enum class CommandOperation
    {
        None,
        Delete,
        Insert,
        Copy,
        Paste
    };
    CommandOperation op = CommandOperation::None;

    auto normalizeOffsets = [&]() {
        // Clamp and orient the correct way around
        startOffset = buffer.Clamp(startOffset);
        endOffset = buffer.Clamp(endOffset);
        if (startOffset > endOffset)
        {
            std::swap(startOffset, endOffset);
        }
    };

    auto extendEndOffset = [&](int count) {
        // Clamp and orient the correct way around
        endOffset = buffer.LocationFromOffsetByChars(endOffset, count);
        endOffset = buffer.Clamp(endOffset);
    };

    if (key == ExtKeys::ESCAPE)
    {
        SwitchMode(EditorMode::Insert);
    }
    // CTRL + ...
    else if (modifierKeys & ModifierKey::Ctrl)
    {
        // Undo
        if (key == 'z')
        {
            Undo();
            return;
        }
        // Redo
        else if (key == 'y')
        {
            Redo();
            return;
        }
        else if (key == ExtKeys::RIGHT)
        {
            auto target = buffer.StandardCtrlMotion(m_visualBegin, bufferCursor, SearchDirection::Forward);
            GetCurrentWindow()->SetBufferCursor(target.second);
        }
        else if (key == ExtKeys::LEFT)
        {
            auto target = buffer.StandardCtrlMotion(m_visualBegin, bufferCursor, SearchDirection::Backward);
            GetCurrentWindow()->SetBufferCursor(target.second);
        }
        else if (key == ExtKeys::HOME)
        {
            // CTRL HOME = top of file
            GetCurrentWindow()->SetBufferCursor(BufferLocation{ 0 });
        }
        else if (key == ExtKeys::END)
        {
            // CTRL END = end of file
            GetCurrentWindow()->SetBufferCursor(buffer.EndLocation());
        }
        // Cut/Copy
        else if (key == 'x')
        {
            // A delete, but also a copy!
            op = CommandOperation::Delete;
            copyRegion = true;
            if (m_currentMode == EditorMode::Visual)
            {
                startOffset = m_visualBegin;
                endOffset = m_visualEnd;
                cursorAfter = startOffset;
                normalizeOffsets();
                extendEndOffset(1);
            }
            else
            {
                lineWise = true;
                cursorAfter = startOffset;
                startOffset = buffer.GetLinePos(bufferCursor, LineLocation::LineBegin);
                endOffset = buffer.GetLinePos(bufferCursor, LineLocation::BeyondLineEnd);
            }
        }
        // Copy
        else if (key == 'c')
        {
            op = CommandOperation::Copy;
            if (m_currentMode == EditorMode::Visual)
            {
                startOffset = m_visualBegin;
                endOffset = m_visualEnd;
                cursorAfter = startOffset;
                normalizeOffsets();
                extendEndOffset(1);
            }
            else
            {
                lineWise = true;
                cursorAfter = startOffset;
                startOffset = buffer.GetLinePos(bufferCursor, LineLocation::LineBegin);
                endOffset = buffer.GetLinePos(bufferCursor, LineLocation::BeyondLineEnd);
            }
        }
        // Paste
        else if (key == 'v')
        {

            bool boundary = false;
            if (m_currentMode == EditorMode::Visual)
            {
                startOffset = m_visualBegin;
                endOffset = m_visualEnd;
                cursorAfter = startOffset;
                normalizeOffsets();
                extendEndOffset(1);

                // Delete existing selection
                auto cmd = std::make_shared<ZepCommand_DeleteRange>(buffer,
                    startOffset,
                    endOffset,
                    startOffset);
                cmd->SetFlags(CommandFlags::GroupBoundary);
                AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
                boundary = true;

                // Next paste from the start of the visual region we just removed
                startOffset = m_visualBegin;
            }
            else
            {
                // Pasting from the cursor position
                startOffset = GetCurrentWindow()->GetBufferCursor();
                endOffset = startOffset;
            }

            auto pRegister = &GetEditor().GetRegister('"');
            if (!pRegister->text.empty())
            {
                cursorAfter = buffer.LocationFromOffset(startOffset, long(Utf8Length(pRegister->text.c_str())));

                // Simple insert
                auto cmd = std::make_shared<ZepCommand_Insert>(buffer,
                    startOffset,
                    pRegister->text,
                    cursorAfter);
                if (boundary)
                {
                    cmd->SetFlags(CommandFlags::GroupBoundary);
                }
                AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
            }
        }
    }
    else if (key == ExtKeys::HOME)
    {
        // Beginning of line
        auto pos = buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar);
        GetCurrentWindow()->SetBufferCursor(pos);
    }
    else if (key == ExtKeys::END)
    {
        auto pos = buffer.GetLinePos(bufferCursor, LineLocation::LineCRBegin);
        GetCurrentWindow()->SetBufferCursor(pos);
    }
    else if (key == ExtKeys::RIGHT)
    {
        GetCurrentWindow()->SetBufferCursor(bufferCursor + 1);
    }
    else if (key == ExtKeys::LEFT)
    {
        GetCurrentWindow()->SetBufferCursor(bufferCursor - 1);
    }
    else if (key == ExtKeys::UP)
    {
        // In standard mode, the cursor might jump back to a shorter line, but we want
        // it 'one' further along because we are always in 'insert' mode (in vim the cursor
        // is effectively on the 'previous' character.  So this fix is just working around that
        // beahvior in the window layer
        // TODO: A cleaner approach?
        auto start_x = GetCurrentWindow()->BufferToDisplay().x;
        GetCurrentWindow()->MoveCursorY(-1);
        if (GetCurrentWindow()->BufferToDisplay().x < start_x)
        {
            GetCurrentWindow()->SetBufferCursor(GetCurrentWindow()->GetBufferCursor() + 1);
        }
    }
    else if (key == ExtKeys::DOWN)
    {
        auto start_x = GetCurrentWindow()->BufferToDisplay().x;
        GetCurrentWindow()->MoveCursorY(1);
        if (GetCurrentWindow()->BufferToDisplay().x < start_x)
        {
            GetCurrentWindow()->SetBufferCursor(GetCurrentWindow()->GetBufferCursor() + 1);
        }
    }
    else if (key == ExtKeys::RETURN)
    {
        ch = "\n";
        op = CommandOperation::Insert;
        cursorAfter = buffer.LocationFromOffsetByChars(startOffset, long(ch.length()));
    }
    else if (key == ExtKeys::TAB)
    {
        // 4 Spaces, obviously :)
        ch = "    ";
        op = CommandOperation::Insert;
        cursorAfter = buffer.LocationFromOffsetByChars(startOffset, long(ch.length()));
    }
    else if (key == ExtKeys::DEL)
    {
        op = CommandOperation::Delete;
        if (m_currentMode == EditorMode::Visual)
        {
            startOffset = m_visualBegin;
            endOffset = m_visualEnd;
            cursorAfter = startOffset;
            normalizeOffsets();
            extendEndOffset(1);
        }
        else
        {
            endOffset = buffer.LocationFromOffsetByChars(startOffset, 1);
            cursorAfter = startOffset;
        }
    }
    else if (key == ExtKeys::BACKSPACE)
    {
        op = CommandOperation::Delete;
        if (m_currentMode == EditorMode::Visual)
        {
            startOffset = m_visualBegin;
            endOffset = m_visualEnd;
            cursorAfter = startOffset;
            normalizeOffsets();
            extendEndOffset(1);
        }
        else
        {
            endOffset = startOffset;
            startOffset = buffer.LocationFromOffsetByChars(startOffset, -1);
            cursorAfter = startOffset;
        }
    }
    else
    {
        op = CommandOperation::Insert;
    }

    // Shift select
    // Turn on visual mode and track the selection area
    if (modifierKeys & ModifierKey::Shift && op == CommandOperation::None)
    {
        if (m_currentMode != EditorMode::Visual)
        {
            m_currentMode = EditorMode::Visual;
            if (GetCurrentWindow()->GetBufferCursor() < startOffset)
            {
                startOffset--;
            }
            m_visualBegin = startOffset;
            m_visualEnd = startOffset;
            GetCurrentWindow()->SetCursorMode(CursorMode::Visual);
        }
    }

    // Op is a copy or also requires the region to be copied
    if (copyRegion || op == CommandOperation::Copy)
    {
        // Grab it
        std::string str = std::string(buffer.GetText().begin() + startOffset, buffer.GetText().begin() + endOffset);
        GetEditor().GetRegister('"').text = str;
        GetEditor().GetRegister('"').lineWise = lineWise;
        GetEditor().GetRegister('0').text = str;
        GetEditor().GetRegister('0').lineWise = lineWise;
    }
    // Not holding shift, so back to insert mode
    else if (!(modifierKeys & ModifierKey::Shift))
    {
        m_currentMode = EditorMode::Insert;
        GetCurrentWindow()->SetCursorMode(CursorMode::Insert);
        m_visualBegin = m_visualEnd = 0;
    }

    // Insert into buffer
    if (op == CommandOperation::Insert)
    {
        // Simple insert
        auto cmd = std::make_shared<ZepCommand_Insert>(buffer,
            startOffset,
            ch,
            cursorAfter);
        AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        m_currentMode = EditorMode::Insert;
        GetCurrentWindow()->SetCursorMode(CursorMode::Insert);
    }
    // Delete from buffer
    else if (op == CommandOperation::Delete)
    {

        // Delete
        auto cmd = std::make_shared<ZepCommand_DeleteRange>(buffer,
            startOffset,
            endOffset,
            cursorAfter != -1 ? cursorAfter : startOffset);
        AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        m_currentMode = EditorMode::Insert;
        GetCurrentWindow()->SetCursorMode(CursorMode::Insert);
    }

    UpdateVisualSelection();
}

} // namespace Zep