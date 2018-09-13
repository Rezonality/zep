#include "utils/stringutils.h"
#include "mode_standard.h"
#include "window.h"
#include "commands.h"

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

// Word motion for standard mode
static BufferLocation WordMotion(const BufferBlock& block)
{
    // If on a space, move to the first block
    // Otherwise, we are on a word, and need to move to the second block
    if (block.direction == 1)
    {
        if (block.spaceBefore)
            return block.firstBlock;
        else
            return block.secondBlock;
    }
    else
    {
        // abc def  If on the 'd', jump to the 'a' 
        if (block.blockSearchPos == (block.firstNonBlock - block.direction))
        {
            return block.secondNonBlock - block.direction;
        }
        // Otherwise, beginning of current word
        return block.firstNonBlock - block.direction;
    }
}

ZepMode_Standard::ZepMode_Standard(ZepEditor& editor)
    : ZepMode(editor)
{
}

ZepMode_Standard::~ZepMode_Standard()
{

}

void ZepMode_Standard::SetCurrentWindow(ZepWindow* pView)
{
    if (m_pCurrentView != pView)
    {
        ZepMode::SetCurrentWindow(pView);
        SwitchMode(EditorMode::Insert);
    }
}

void ZepMode_Standard::Begin()
{
    SwitchMode(EditorMode::Insert);
}

void ZepMode_Standard::SwitchMode(EditorMode mode)
{
    assert(mode == EditorMode::Insert || mode == EditorMode::Visual);
    m_currentMode = mode;
    if (m_pCurrentView)
    {
        m_pCurrentView->SetCursorMode(mode == EditorMode::Insert ? CursorMode::Insert : CursorMode::Visual);
    }
}

void ZepMode_Standard::AddKeyPress(uint32_t key, uint32_t modifierKeys)
{
    std::string ch((char*)&key);

    bool copyRegion = false;
    bool pasteText = false;
    bool lineWise = false;
    auto& buffer = m_pCurrentView->GetBuffer();
    BufferLocation startOffset = m_pCurrentView->DisplayToBuffer();
    BufferLocation endOffset = buffer.LocationFromOffsetByChars(startOffset, long(ch.length()));
    BufferLocation cursorAfter = endOffset;

    const auto cursor = m_pCurrentView->GetCursor();
    const auto bufferCursor = m_pCurrentView->DisplayToBuffer(cursor);
    const LineInfo* pLineInfo = nullptr;
    if (m_pCurrentView->visibleLines.size() > cursor.y)
    {
        pLineInfo = &m_pCurrentView->visibleLines[cursor.y];
    }

    enum class CommandOperation
    {
        None,
        Delete,
        Insert,
        Copy,
        Paste
    };
    CommandOperation op = CommandOperation::None;

    if (key == ExtKeys::ESCAPE)
    {
        SwitchMode(EditorMode::Insert);
    }
    else if (key == 'x' && (modifierKeys & ModifierKey::Ctrl))
    {
        op = CommandOperation::Delete;
        copyRegion = true;
        if (m_currentMode == EditorMode::Visual)
        {
            startOffset = m_visualBegin;
            endOffset = m_visualEnd;
            cursorAfter = startOffset;
        }
        else
        {
            lineWise = true;
            cursorAfter = startOffset;
            startOffset = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
            endOffset = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
        }
    }
    else if (key == 'c' && (modifierKeys & ModifierKey::Ctrl))
    {
        copyRegion = true;
        if (m_currentMode == EditorMode::Visual)
        {
            startOffset = m_visualBegin;
            endOffset = m_visualEnd;
            cursorAfter = startOffset;
        }
        else
        {
            lineWise = true;
            cursorAfter = startOffset;
            startOffset = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
            endOffset = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
        }
    }
    else if (key == 'v' && (modifierKeys & ModifierKey::Ctrl))
    {
        bool boundary = false;
        if (m_currentMode == EditorMode::Visual)
        {
            // Delete existing selection
            auto cmd = std::make_shared<ZepCommand_DeleteRange>(buffer,
                m_visualBegin,
                m_visualEnd,
                m_visualBegin
                );
            cmd->SetFlags(CommandFlags::GroupBoundary);
            AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
            boundary = true;
            startOffset = m_visualBegin;
        }

        auto pRegister = &GetEditor().GetRegister('"');
        if (!pRegister->text.empty())
        {
            cursorAfter = buffer.LocationFromOffset(startOffset, long(StringUtils::Utf8Length(pRegister->text.c_str())) - 1);
        
            // Simple insert
            auto cmd = std::make_shared<ZepCommand_Insert>(buffer,
                startOffset,
                pRegister->text,
                cursorAfter
                );
            if (boundary)
            {
                cmd->SetFlags(CommandFlags::GroupBoundary);
            }
            AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        }
    }
    else if (key == ExtKeys::HOME)
    {
        if (modifierKeys & ModifierKey::Ctrl)
        {
            m_pCurrentView->MoveCursorTo(BufferLocation{ 0 });
        }
        else
        {
            auto pos = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineFirstGraphChar);
            m_pCurrentView->MoveCursorTo(pos);
        }
    }
    else if (key == ExtKeys::END)
    {
        if (modifierKeys & ModifierKey::Ctrl)
        {
            m_pCurrentView->MoveCursorTo(buffer.EndLocation(), LineLocation::LineEnd);
        }
        else
        {
            auto pos = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
            m_pCurrentView->MoveCursorTo(pos, LineLocation::LineEnd);
        }
    }
    else if (key == ExtKeys::RIGHT)
    {
        if (modifierKeys & ModifierKey::Ctrl)
        {
            auto block = buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
            m_pCurrentView->MoveCursorTo(WordMotion(block), Zep::LineLocation::LineEnd);
        }
        else
        {
            if (cursor.x == pLineInfo->Length() - 1)
            {
                m_pCurrentView->MoveCursor(Zep::NVec2i(-MaxCursorMove, 1), Zep::LineLocation::LineEnd);
            }
            m_pCurrentView->MoveCursor(Zep::NVec2i(1, 0), Zep::LineLocation::LineEnd);
        }
    }
    else if (key == ExtKeys::LEFT)
    {
        if (modifierKeys & ModifierKey::Ctrl)
        {
            auto block = buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Backward);
            m_pCurrentView->MoveCursorTo(WordMotion(block));
        }
        else
        {
            if (cursor.x == 0)
            {
                m_pCurrentView->MoveCursor(Zep::NVec2i(MaxCursorMove, -1), Zep::LineLocation::LineEnd);
            }
            else
            {
                m_pCurrentView->MoveCursor(Zep::NVec2i(-1, 0), Zep::LineLocation::LineEnd);
            }
        }
    }
    else if (key == ExtKeys::UP)
    {
        m_pCurrentView->MoveCursor(Zep::NVec2i(0, -1));
    }
    else if (key == ExtKeys::DOWN)
    {
        m_pCurrentView->MoveCursor(Zep::NVec2i(0, 1));
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

    if (modifierKeys & ModifierKey::Ctrl)
    {
        if (ch == "z")
        {
            Undo();
            return;
        }
        else if (ch == "y")
        {
            Redo();
            return;
        }
    }

    if (modifierKeys & ModifierKey::Shift &&
        op == CommandOperation::None)
    {
        if (m_currentMode != EditorMode::Visual)
        {
            m_currentMode = EditorMode::Visual;
            m_visualBegin = startOffset;
            m_visualEnd = startOffset;
            m_pCurrentView->SetCursorMode(CursorMode::Visual);
        }
    }
    else
    {
        if (op == CommandOperation::Insert)
        {
            m_currentMode = EditorMode::Insert;
            m_pCurrentView->SetCursorMode(CursorMode::Insert);
        }
    }

    if (copyRegion)
    {
        // Grab it
        std::string str = std::string(buffer.GetText().begin() + startOffset, buffer.GetText().begin() + endOffset);
        GetEditor().GetRegister('"').text = str;
        GetEditor().GetRegister('"').lineWise = lineWise;
        GetEditor().GetRegister('0').text = str;
        GetEditor().GetRegister('0').lineWise = lineWise;
        m_currentMode = EditorMode::Insert;
        m_pCurrentView->SetCursorMode(CursorMode::Insert);
    }

    if (op == CommandOperation::Insert)
    {
        // Simple insert
        auto cmd = std::make_shared<ZepCommand_Insert>(buffer,
            startOffset,
            ch,
            cursorAfter
            );
        AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        m_currentMode = EditorMode::Insert;
        m_pCurrentView->SetCursorMode(CursorMode::Insert);
    }
    else if (op == CommandOperation::Delete)
    {

        // Delete 
        auto cmd = std::make_shared<ZepCommand_DeleteRange>(buffer,
            startOffset,
            endOffset,
            cursorAfter != -1 ? cursorAfter : startOffset
            );
        AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        m_currentMode = EditorMode::Insert;
        m_pCurrentView->SetCursorMode(CursorMode::Insert);
    }

    m_pCurrentView->GetDisplay().ResetCursorTimer();

    UpdateVisualSelection();
}

} // Zep namespace