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
    // This will also set the cursor type
    SwitchMode(EditorMode::Insert);
}

bool ZepMode_Standard::SwitchMode(EditorMode mode)
{
    assert(mode == EditorMode::Insert || mode == EditorMode::Visual);
    if (mode == m_currentMode)
    {
        return false;
    }

    m_currentMode = mode;

    if (GetCurrentWindow())
    {
        // Cursor is always in insert mode for standard
        GetCurrentWindow()->SetCursorType(CursorType::Insert);
    }

    if (mode == EditorMode::Insert)
    {
        m_visualBegin = m_visualEnd = 0;
    }
    else
    {
        m_visualBegin = GetCurrentWindow()->GetBufferCursor();
        m_visualEnd = m_visualBegin;
    }
    return true;
}

void ZepMode_Standard::AddKeyPress(uint32_t key, uint32_t modifierKeys)
{
    std::string ch(1, (char)key);

    bool copyRegion = false;
    bool lineWise = false;
    auto& buffer = GetCurrentWindow()->GetBuffer();
    BufferLocation bufferCursor = GetCurrentWindow()->GetBufferCursor();
    BufferLocation startOffset = bufferCursor;
    BufferLocation endOffset = buffer.LocationFromOffsetByChars(bufferCursor, long(ch.length()));

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

    if (key == ExtKeys::ESCAPE)
    {
        SwitchMode(EditorMode::Insert);
        return;
    }

    bool begin_shift = false;
    bool return_to_insert = false;
    switch (key)
    {
        case ExtKeys::DOWN:
        case ExtKeys::UP:
        case ExtKeys::LEFT:
        case ExtKeys::RIGHT:
        case ExtKeys::END:
        case ExtKeys::HOME:
        case ExtKeys::PAGEDOWN:
        case ExtKeys::PAGEUP:
            if (modifierKeys & ModifierKey::Shift)
            {
                begin_shift = SwitchMode(EditorMode::Visual);
            }
            else
            {
                // Immediate switch back to insert
                SwitchMode(EditorMode::Insert);
            }
            break;
        default:
            return_to_insert = true;
            break;
    }

    // CTRL + ...
    if (modifierKeys & ModifierKey::Ctrl)
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
        // Motions fall through to selection code
        else if (key == ExtKeys::RIGHT)
        {
            auto target = buffer.StandardCtrlMotion(bufferCursor, SearchDirection::Forward);
            GetCurrentWindow()->SetBufferCursor(target.second);
        }
        else if (key == ExtKeys::LEFT)
        {
            auto target = buffer.StandardCtrlMotion(bufferCursor, SearchDirection::Backward);
            GetCurrentWindow()->SetBufferCursor(target.second);
        }
        else if (key == ExtKeys::HOME)
        {
            // CTRL HOME = top of file
            GetCurrentWindow()->SetBufferCursor(BufferLocation{0});
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
                normalizeOffsets();
            }
            else
            {
                lineWise = true;
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
                normalizeOffsets();
                return_to_insert = false;
            }
            else
            {
                lineWise = true;
                startOffset = buffer.GetLinePos(bufferCursor, LineLocation::LineBegin);
                endOffset = buffer.GetLinePos(bufferCursor, LineLocation::BeyondLineEnd);
            }
        }
        // Paste
        else if (key == 'v')
        {
            auto pRegister = &GetEditor().GetRegister('"');
            if (!pRegister->text.empty())
            {
                ch = pRegister->text;
            }
            else
            {
                ch.clear();
            }
            op = CommandOperation::Insert;
            startOffset = bufferCursor;
            endOffset = startOffset + BufferLocation(ch.length());
        }
    }
    else if (key == ExtKeys::HOME)
    {
        // Beginning of line
        auto pos = buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar);
        GetCurrentWindow()->SetBufferCursor(pos);
        GetEditor().ResetCursorTimer();
    }
    else if (key == ExtKeys::END)
    {
        auto pos = buffer.GetLinePos(bufferCursor, LineLocation::LineCRBegin);
        GetCurrentWindow()->SetBufferCursor(pos);
        GetEditor().ResetCursorTimer();
    }
    else if (key == ExtKeys::RIGHT)
    {
        GetCurrentWindow()->SetBufferCursor(bufferCursor + 1);
        GetEditor().ResetCursorTimer();
    }
    else if (key == ExtKeys::LEFT)
    {
        GetCurrentWindow()->SetBufferCursor(bufferCursor - 1);
        GetEditor().ResetCursorTimer();
    }
    else if (key == ExtKeys::UP)
    {
        GetCurrentWindow()->MoveCursorY(-1, LineLocation::LineCRBegin);
    }
    else if (key == ExtKeys::DOWN)
    {
        GetCurrentWindow()->MoveCursorY(1, LineLocation::LineCRBegin);
    }
    else if (key == ExtKeys::RETURN)
    {
        ch = "\n";
        op = CommandOperation::Insert;
    }
    else if (key == ExtKeys::TAB)
    {
        // 4 Spaces, obviously :)
        ch = "    ";
        op = CommandOperation::Insert;
    }
    else if (key == ExtKeys::DEL)
    {
        op = CommandOperation::Delete;
        if (m_currentMode == EditorMode::Visual)
        {
            startOffset = m_visualBegin;
            endOffset = m_visualEnd;
            normalizeOffsets();
        }
        else
        {
            endOffset = buffer.LocationFromOffsetByChars(startOffset, 1);
        }
    }
    else if (key == ExtKeys::BACKSPACE)
    {
        op = CommandOperation::Delete;
        if (m_currentMode == EditorMode::Visual)
        {
            startOffset = m_visualBegin;
            endOffset = m_visualEnd;
            normalizeOffsets();
        }
        else
        {
            endOffset = startOffset;
            startOffset = buffer.LocationFromOffsetByChars(startOffset, -1);
        }
    }
    else
    {
        op = CommandOperation::Insert;
    }

    startOffset = buffer.Clamp(startOffset);
    endOffset = buffer.Clamp(endOffset);

    // Here we handle the logic of wether the visual range starts or ends before or
    // after the cursor!  It depends on which direction the user moved when they started the selection
    if (m_currentMode == EditorMode::Visual)
    {
        if (begin_shift)
        {
            if (GetCurrentWindow()->GetBufferCursor() <= startOffset)
            {
                m_visualBegin = startOffset; // buffer.LocationFromOffsetByChars(startOffset, -1);
            }
            else
            {
                m_visualBegin = startOffset;
            }
        }
        if (GetCurrentWindow()->GetBufferCursor() > m_visualBegin)
        {
            m_visualEnd = GetCurrentWindow()->GetBufferCursor();
        }
        else
        {
            m_visualEnd = GetCurrentWindow()->GetBufferCursor(); // buffer.LocationFromOffsetByChars(GetCurrentWindow()->GetBufferCursor(), -1);
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

        // Copy doesn't clear the visual seletion
        return_to_insert = false;
    }
    // Insert into buffer
    else if (op == CommandOperation::Insert)
    {
        bool boundary = false;
        if (m_currentMode == EditorMode::Visual)
        {
            startOffset = m_visualBegin;
            endOffset = m_visualEnd;
            normalizeOffsets();

            // Delete existing selection
            auto cmd = std::make_shared<ZepCommand_DeleteRange>(buffer, startOffset, endOffset, GetCurrentWindow()->GetBufferCursor());
            if (!ch.empty())
            {
                cmd->SetFlags(CommandFlags::GroupBoundary);
            }
            AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
            boundary = true;
        }

        // Simple insert
        if (!ch.empty())
        {
            auto cmd = std::make_shared<ZepCommand_Insert>(buffer, startOffset, ch, GetCurrentWindow()->GetBufferCursor());
            if (boundary)
            {
                cmd->SetFlags(CommandFlags::GroupBoundary);
            }
            AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        }
        return_to_insert = true;
    }
    // Delete from buffer
    else if (op == CommandOperation::Delete)
    {
        // Delete
        auto cmd = std::make_shared<ZepCommand_DeleteRange>(buffer, startOffset, endOffset, GetCurrentWindow()->GetBufferCursor());
        AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        return_to_insert = true;
    }

    if (return_to_insert)
    {
        SwitchMode(EditorMode::Insert);
    }

    if (m_currentMode == EditorMode::Visual)
    {
        buffer.SetSelection(BufferRange{m_visualBegin, m_visualEnd});
    }
}

} // namespace Zep
