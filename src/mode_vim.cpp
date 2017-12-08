#include <cctype>
#include <sstream>

#include "mode_vim.h"
#include "commands.h"
#include "utils/stringutils.h"
#include "utils/timer.h"

// Note:
// This is a very basic implementation of the common Vim commands that I use: the bare minimum I can live with.
// I do use more, and depending on how much pain I suffer, will add them over time.
// My aim is to make it easy to add commands, so if you want to put something in, please send me a PR.
// The buffer/display search and find support makes it easy to gather the info you need, and the basic insert/delete undo redo commands
// make it easy to find the locations in the buffer
// Important to note: I'm not trying to beat/better Vim here.  Just make an editor I can use in a viewport without feeling pain ;)
// See further down for what is implemented, and what's on my todo list

// IMPLEMENTED VIM:
// Command counts
// hjkl Motions   
// . dot command
// TAB
// w,W,e,E,ge,gE,b,B Word motions
// u,CTRL+r  Undo, Redo
// i,I,a,A Insert mode (pending undo/redo fix)
// DELETE/BACKSPACE in insert and normal mode; match vim
// Command status bar
// Arrow keys
// '$'
// 'jk' to insert mode 
// gg Jump to end
// G Jump to beginning
// 'J' join
// D
// dd,d$,x  Delete line, to end of line, chars
// 'v' + 'x'/'d'
// 'y'
// 'p'/'P'
// a-z&a-Z, 0->9, _ " registers
// '$'
// 'yy'
// cc
// c$  Change to end of line
// C
// S, s, with visual mode
// ^
// 'O', 'o'
// 'V' (linewise v)
// Y, D, linewise yank/paste
// d[a]<count>w/e  Delete words
// di})]"'
// c[a]<count>w/e  Change word
// ci})]"'

// VIM TODO:
// % Jump
// f (find) / next, previous
// /Searching
// visual-repeat (dot command should use last visual selection range)

// 'R'/'r' overstrike

// Standardize buffer format (remove \r\n, use just \n)
// Standard Mode

namespace
{

}
namespace Zep
{

// Given a searched block, find the next word 
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

BufferLocation WordEndMotion(const BufferBlock& block)
{
    // If on a space, move to the first block
    // Otherwise, we are on a word, and need to move to the second block
    if (block.direction == 1)
    {
        // If we are sitting on the end of the block, move to the next one
        if (block.blockSearchPos == block.firstNonBlock - block.direction)
        {
            return block.secondNonBlock - block.direction;
        }
        else
        {
            return block.firstNonBlock - block.direction;
        }
    }
    else
    {
        // 'ge'
        // Back to the end of the word
        if (block.spaceBefore)
        {
            return block.firstBlock;
        }
        else
        {
            return block.secondBlock;
        }
    }
}

std::pair<BufferLocation, BufferLocation> Word(const BufferBlock& block)
{
    if (block.spaceBefore)
    {
        return std::make_pair(block.blockSearchPos, block.firstNonBlock);
    }
    else
    {
        return std::make_pair(block.firstBlock, block.secondBlock);
    }
}

std::pair<BufferLocation, BufferLocation> InnerWord(const BufferBlock& block)
{
    if (block.spaceBefore)
    {
        return std::make_pair(block.spaceBeforeStart, block.firstBlock);
    }
    return std::make_pair(block.firstBlock, block.firstNonBlock);
}

ZepMode_Vim::ZepMode_Vim(ZepEditor& editor)
    : ZepMode(editor)
{
    Init();
}

ZepMode_Vim::~ZepMode_Vim()
{

}

void ZepMode_Vim::Init()
{
    for (int i = 0; i <= 9; i++)
    {
        GetEditor().SetRegister('0' + i, "");
    }
    GetEditor().SetRegister('"', "");
}

void ZepMode_Vim::ResetCommand()
{
    m_currentCommand.clear();
}

void ZepMode_Vim::SwitchMode(EditorMode mode)
{
    // Don't switch to invalid mode
    if (mode == EditorMode::None)
        return;

    m_currentMode = mode;
    switch (mode)
    {
    case EditorMode::Normal:
        m_pCurrentWindow->SetCursorMode(CursorMode::Normal);
        ResetCommand();
        break;
    case EditorMode::Insert:
        m_insertBegin = m_pCurrentWindow->DisplayToBuffer();
        m_pCurrentWindow->SetCursorMode(CursorMode::Insert);
        m_pendingEscape = false;
        break;
    case EditorMode::Visual:
        m_pCurrentWindow->SetCursorMode(CursorMode::Visual);
        ResetCommand();
        m_pendingEscape = false;
        break;
    default:
    case EditorMode::Command:
    case EditorMode::None:
        break;
    }
}

bool ZepMode_Vim::GetBlockOpRange(const std::string& op, EditorMode mode, BufferLocation& beginRange, BufferLocation& endRange, BufferLocation& cursorAfter) const
{
    auto pBuffer = m_pCurrentWindow->m_pCurrentBuffer;
    const auto cursor = m_pCurrentWindow->GetCursor();
    const auto bufferCursor = m_pCurrentWindow->DisplayToBuffer(cursor);
    const LineInfo* pLineInfo = nullptr;
    if (m_pCurrentWindow->visibleLines.size() > cursor.y)
    {
        pLineInfo = &m_pCurrentWindow->visibleLines[cursor.y];
    }

    beginRange = BufferLocation{ -1 };
    if (op == "visual")
    {
        if (mode == EditorMode::Visual)
        {
            beginRange = m_visualBegin;
            endRange = pBuffer->LocationFromOffsetByChars(m_visualEnd, 1);
            cursorAfter = beginRange;
        }
    }
    else if (op == "line")
    {
        // Whole line
        if (pLineInfo)
        {
            beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
            endRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
            cursorAfter = beginRange;
        }
    }
    else if (op == "$")
    {
        if (pLineInfo)
        {
            beginRange = bufferCursor;
            endRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineCRBegin);
            cursorAfter = beginRange;
        }
    }
    else if (op == "w")
    {
        auto block = pBuffer->GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = block.blockSearchPos;
        endRange = WordMotion(block);
        cursorAfter = beginRange;
    }
    else if (op == "W")
    {
        auto block = pBuffer->GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = block.blockSearchPos;
        endRange = WordMotion(block);
        cursorAfter = beginRange;
    }
    else if (op == "aw")
    {
        auto block = pBuffer->GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = Word(block).first;
        endRange = Word(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "aW")
    {
        auto block = pBuffer->GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = Word(block).first;
        endRange = Word(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "iw")
    {
        auto block = pBuffer->GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = InnerWord(block).first;
        endRange = InnerWord(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "iW")
    {
        auto block = pBuffer->GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = InnerWord(block).first;
        endRange = InnerWord(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "cursor")
    {
        beginRange = bufferCursor;
        endRange = pBuffer->LocationFromOffsetByChars(bufferCursor, 1);
        cursorAfter = bufferCursor;
    }
    return beginRange != -1;
}

bool ZepMode_Vim::GetCommand(std::string command, uint32_t lastKey, uint32_t modifierKeys, EditorMode mode, int count, CommandResult& commandResult)
{
    auto cursor = m_pCurrentWindow->GetCursor();
    const LineInfo* pLineInfo = nullptr;
    if (m_pCurrentWindow->visibleLines.size() > cursor.y)
    {
        pLineInfo = &m_pCurrentWindow->visibleLines[cursor.y];
    }
    enum class CommandOperation
    {
        None,
        Delete,
        DeleteLines,
        Insert,
        Copy,
        CopyLines
    };

    commandResult = CommandResult{};
    BufferLocation beginRange{ -1 };
    BufferLocation endRange{ -1 };
    BufferLocation cursorAfter{ -1 };
    std::stack<char> registers;
    registers.push('"');
    CommandOperation op = CommandOperation::None;
    Register tempReg("", false);
    const Register* pRegister = &tempReg;

    auto pBuffer = m_pCurrentWindow->m_pCurrentBuffer;
    const auto bufferCursor = m_pCurrentWindow->DisplayToBuffer(cursor);

    // Store the register source
    if (command[0] == '"' && command.size() > 2)
    {
        // Null register
        if (command[1] == '_')
        {
            std::stack<char> temp;
            registers.swap(temp);
        }
        else
        {
            registers.push(command[1]);
            char reg = command[1];

            // Demote capitals to lower registers when pasting (all both)
            if (reg >= 'A' && reg <= 'Z')
            {
                reg = std::tolower(reg);
            }

            if (GetEditor().GetRegisters().find(std::string({ reg })) != GetEditor().GetRegisters().end())
            {
                pRegister = &GetEditor().GetRegister(reg);
            }
        }
        command = command.substr(2);
    }
    else
    {
        // Default register
        if (pRegister->text.empty())
            pRegister = &GetEditor().GetRegister('"');
    }

    // Motion
    if (command == "$")
    {
        m_pCurrentWindow->MoveCursorTo(pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineCRBegin));
        return true;
    }
    else if (command == "0")
    {
        m_pCurrentWindow->MoveCursorTo(pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin));
        return true;
    }
    else if (command == "^")
    {
        m_pCurrentWindow->MoveCursorTo(pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineFirstGraphChar));
        return true;
    }
    else if (command == "j" || command == "+" || lastKey == ExtKeys::DOWN)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, count));
        commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (command == "k" || command == "-" || lastKey == ExtKeys::UP)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, -count));
        commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (command == "l" || lastKey == ExtKeys::RIGHT)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(count, 0));
        commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (command == "h" || lastKey == ExtKeys::LEFT)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(-count, 0));
        commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (command == "G")
    {
        if (count != 1)
        {
            // Goto line
            m_pCurrentWindow->MoveCursorTo(pBuffer->GetLinePos(count, LineLocation::LineBegin));
            commandResult.flags |= CommandResultFlags::HandledCount;
        }
        else
        {
            m_pCurrentWindow->MoveCursorTo(pBuffer->EndLocation());
            commandResult.flags |= CommandResultFlags::HandledCount;
        }
        return true;
    }
    else if (lastKey == ExtKeys::BACKSPACE)
    {
        auto loc = bufferCursor;

        // Insert-mode command
        if (mode == EditorMode::Insert)
        {
            // In insert mode, we are 'on' the character after the one we want to delete
            beginRange = pBuffer->LocationFromOffsetByChars(loc, -1);
            endRange = pBuffer->LocationFromOffsetByChars(loc, 0);
            cursorAfter = beginRange;
            op = CommandOperation::Delete;
        }
        else
        {
            // Normal mode moves over the chars, and wraps
            m_pCurrentWindow->MoveCursorTo(pBuffer->LocationFromOffsetByChars(loc, -1));
            return true;
        }
    }
    else if (command == "w")
    {
        auto block = pBuffer->GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (command == "W")
    {
        auto block = pBuffer->GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (command == "b")
    {
        auto block = pBuffer->GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Backward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (command == "B")
    {
        auto block = pBuffer->GetBlock(SearchType::Word, bufferCursor, SearchDirection::Backward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (command == "e")
    {
        auto block = pBuffer->GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
        return true;
    }
    else if (command == "E")
    {
        auto block = pBuffer->GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
        return true;
    }
    else if (command[0] == 'g')
    {
        if (command == "ge")
        {
            auto block = pBuffer->GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Backward);
            m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
        }
        else if (command == "gE")
        {
            auto block = pBuffer->GetBlock(SearchType::Word, bufferCursor, SearchDirection::Backward);
            m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
        }
        else if (command == "gg")
        {
            m_pCurrentWindow->MoveCursorTo(BufferLocation{ 0 });
        }
        else
        {
            return false;
        }
        return true;
    }
    else if (command == "J")
    {
        // Delete the CR (and thus join lines)
        beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineCRBegin);
        endRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
        cursorAfter = bufferCursor;
        op = CommandOperation::Delete;
    }
    else if (command == "v" ||
        command == "V")
    {
        if (m_currentMode == EditorMode::Visual)
        {
            commandResult.modeSwitch = EditorMode::Normal;
        }
        else
        {
            if (command == "V")
            {
                m_visualBegin = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
                m_visualEnd = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd) - 1;
            }
            else
            {
                m_visualBegin = bufferCursor;
                m_visualEnd = m_visualBegin;
            }
            commandResult.modeSwitch = EditorMode::Visual;
        }
        m_lineWise = command == "V" ? true : false;
        return true;
    }
    else if (command == "x" || lastKey == ExtKeys::DEL)
    {
        auto loc = bufferCursor;

        if (m_currentMode == EditorMode::Visual)
        {
            beginRange = m_visualBegin;
            endRange = pBuffer->LocationFromOffsetByChars(m_visualEnd, 1);
            cursorAfter = m_visualBegin;
            op = CommandOperation::Delete;
            commandResult.modeSwitch = EditorMode::Normal;
        }
        else
        {
            // Don't allow x to delete beyond the end of the line
            if (command != "x" ||
                std::isgraph(pBuffer->GetText()[loc]) ||
                std::isblank(pBuffer->GetText()[loc]))
            {
                beginRange = loc;
                endRange = pBuffer->LocationFromOffsetByChars(loc, 1);
                cursorAfter = loc;
                op = CommandOperation::Delete;
            }
            else
            {
                ResetCommand();
            }
        }
    }
    else if (command[0] == 'o')
    {
        beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
        tempReg.text = "\n";
        pRegister = &tempReg;
        cursorAfter = beginRange;
        op = CommandOperation::Insert;
        commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (command[0] == 'O')
    {
        beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
        tempReg.text = "\n";
        pRegister = &tempReg;
        cursorAfter = beginRange;
        op = CommandOperation::Insert;
        commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (command[0] == 'd' ||
        command == "D")
    {
        if (command == "d")
        {
            // Only in visual mode; delete selected block
            if (GetBlockOpRange("visual", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
                commandResult.modeSwitch = EditorMode::Normal;
            }
        }
        else if (command == "dd")
        {
            if (GetBlockOpRange("line", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::DeleteLines;
                commandResult.modeSwitch = EditorMode::Normal;
            }
        }
        else if (command == "d$" ||
            command == "D")
        {
            if (GetBlockOpRange("$", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "dw")
        {
            if (GetBlockOpRange("w", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "dW")
        {
            if (GetBlockOpRange("W", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "daw")
        {
            if (GetBlockOpRange("aw", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "daW")
        {
            if (GetBlockOpRange("aW", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "diw")
        {
            if (GetBlockOpRange("iw", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "diW")
        {
            if (GetBlockOpRange("iW", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
    }
    // Substitute
    else if ((command[0] == 's') ||
        (command[0] == 'S'))
    {
        if (command == "S")
        {
            if (pLineInfo)
            {
                // Delete whole line and go to insert mode
                beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
                endRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineCRBegin);
                cursorAfter = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineFirstGraphChar);
                op = CommandOperation::Delete;
            }
        }
        else if (command == "s")
        {
            // Only in visual mode; delete selected block and go to insert mode
            if (GetBlockOpRange("visual", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
            // Just delete under cursor and insert
            else if(GetBlockOpRange("cursor", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else
        {
            return false;
        }
        commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (command[0] == 'C' ||
        command[0] == 'c')
    {
        if (command == "c")
        {
            // Only in visual mode; delete selected block
            if (GetBlockOpRange("visual", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "cc")
        {
            if (GetBlockOpRange("line", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::DeleteLines;
            }
        }
        else if (command == "c$" ||
            command == "C")
        {
            if (GetBlockOpRange("$", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "cw")
        {
            if (GetBlockOpRange("w", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "cW")
        {
            if (GetBlockOpRange("W", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "caw")
        {
            if (GetBlockOpRange("aw", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "caW")
        {
            if (GetBlockOpRange("aW", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "ciw")
        {
            if (GetBlockOpRange("iw", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }
        else if (command == "ciW")
        {
            if (GetBlockOpRange("iW", mode, beginRange, endRange, cursorAfter))
            {
                op = CommandOperation::Delete;
            }
        }

        if (op != CommandOperation::None)
        {
            commandResult.modeSwitch = EditorMode::Insert;
        }
    }
    else if (command == "p")
    {
        if (!pRegister->text.empty())
        {
            if (pRegister->lineWise)
            {
                beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
                cursorAfter = beginRange;
            }
            else
            {
                beginRange = pBuffer->LocationFromOffsetByChars(bufferCursor, 1);
                cursorAfter = pBuffer->LocationFromOffset(beginRange, long(StringUtils::Utf8Length(pRegister->text.c_str())) - 1);
            }
            op = CommandOperation::Insert;
        }
    }
    else if (command == "P")
    {
        if (!pRegister->text.empty())
        {
            if (pRegister->lineWise)
            {
                beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
                cursorAfter = beginRange;
            }
            else
            {
                beginRange = bufferCursor;
                cursorAfter = pBuffer->LocationFromOffsetByChars(beginRange, long(StringUtils::Utf8Length(pRegister->text.c_str()) - 1));
            }
            op = CommandOperation::Insert;
        }
    }
    else if (command[0] == 'y')
    {
        if (mode == EditorMode::Visual)
        {
            registers.push('0');
            beginRange = m_visualBegin;
            endRange = pBuffer->LocationFromOffsetByChars(m_visualEnd, 1);
            cursorAfter = m_visualBegin;
            commandResult.modeSwitch = EditorMode::Normal;
            op = m_lineWise ? CommandOperation::CopyLines : CommandOperation::Copy;
        }
        else if (mode == EditorMode::Normal)
        {
            if (command == "yy")
            {
                if (pLineInfo)
                {
                    // Copy the whole line, including the CR
                    registers.push('0');
                    beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
                    endRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd) + 1;
                    op = CommandOperation::CopyLines;
                }
            }
        }

        if (op == CommandOperation::None)
        {
            return false;
        }
    }
    else if (command == "Y")
    {
        if (pLineInfo)
        {
            // Copy the whole line, including the CR
            registers.push('0');
            beginRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
            endRange = pBuffer->GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
            op = CommandOperation::CopyLines;
            commandResult.modeSwitch = EditorMode::Normal;
        }
    }
    else if (command == "u")
    {
        Undo();
        return true;
    }
    else if (command == "r" && modifierKeys == ModifierKey::Ctrl)
    {
        Redo();
        return true;
    }
    else if (command == "i")
    {
        commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (command == "a")
    {
        // Cursor append
        m_pCurrentWindow->MoveCursor(NVec2i(1, 0), LineLocation::LineCRBegin);
        commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (command == "A")
    {
        // Cursor append to end of line
        m_pCurrentWindow->MoveCursor(LineLocation::LineCRBegin);
        commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (command == "I")
    {
        // Cursor Insert beginning char of line 
        m_pCurrentWindow->MoveCursor(LineLocation::LineFirstGraphChar);
        commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (lastKey == ExtKeys::RETURN)
    {
        if (command[0] == ':')
        {
            if (command == ":reg")
            {
                std::ostringstream str;
                str << "--- Registers ---" << '\n';
                for (auto& reg : GetEditor().GetRegisters())
                {
                    if (!reg.second.text.empty())
                    {
                        std::string displayText = reg.second.text;
                        displayText = StringUtils::ReplaceString(displayText, "\r\n", "^J");
                        str << "\"" << reg.first << "   " << displayText << '\n';
                    }
                }
                m_pCurrentWindow->GetDisplay().SetCommandText(str.str());
                return true;
            }
            else if (command == ":ls")
            {
                std::ostringstream str;
                str << "--- Buffers ---" << '\n';
                int index = 0;
                for (auto& buffer : GetEditor().GetBuffers())
                {
                    if (!buffer->GetName().empty())
                    {
                        std::string displayText = buffer->GetName();
                        displayText = StringUtils::ReplaceString(displayText, "\r\n", "^J");
                        if (pBuffer == buffer.get())
                        {
                            str << "*";
                        }
                        else
                        {
                            str << " ";
                        }
                        str << index++ << " : " << displayText << '\n';
                    }
                }
                m_pCurrentWindow->GetDisplay().SetCommandText(str.str());
                return true;
            }
            else if (command.find(":bu") == 0)
            {
                auto strTok = StringUtils::Split(command, " ");

                if (strTok.size() > 1)
                {
                    try
                    {
                        auto index = std::stoi(strTok[1]);
                        auto current = 0;
                        for (auto& buffer : GetEditor().GetBuffers())
                        {
                            if (index == current)
                            {
                                m_pCurrentWindow->SetCurrentBuffer(buffer.get());
                            }
                            current++;
                        }
                    }
                    catch (std::exception&)
                    {
                    }
                }
                return true;
            }
            else if (!GetEditor().Broadcast(std::make_shared<ZepMessage>(Msg_HandleCommand, command)))
            {
                m_pCurrentWindow->GetDisplay().SetCommandText("Not a command");
                return true;
            }

            m_currentCommand.clear();
            return false;
        }
        return false;
    }
    else
    {
        // Unknown, keep trying
        return false;
    }

    // Store in a register
    if (!registers.empty())
    {
        if (op == CommandOperation::Delete ||
            op == CommandOperation::DeleteLines)
        {
            assert(beginRange <= endRange);
            std::string str = std::string(pBuffer->GetText().begin() + beginRange, pBuffer->GetText().begin() + endRange);

            // Delete commands fill up 1-9 registers
            if (command[0] == 'd' ||
                command[0] == 'D')
            {
                for (int i = 9; i > 1; i--)
                {
                    GetEditor().SetRegister('0' + i, GetEditor().GetRegister('0' + i - 1));
                }
                GetEditor().SetRegister('1', Register(str, (op == CommandOperation::DeleteLines)));
            }

            // Fill up any other required registers
            while (!registers.empty())
            {
                GetEditor().SetRegister(registers.top(), Register(str, (op == CommandOperation::DeleteLines)));
                registers.pop();
            }
        }
        else if (op == CommandOperation::Copy ||
            op == CommandOperation::CopyLines)
        {
            std::string str = std::string(pBuffer->GetText().begin() + beginRange, pBuffer->GetText().begin() + endRange);
            while (!registers.empty())
            {
                // Capital letters append to registers instead of replacing them
                if (registers.top() >= 'A' && registers.top() <= 'Z')
                {
                    auto chlow = std::tolower(registers.top());
                    GetEditor().GetRegister(chlow).text += str;
                    GetEditor().GetRegister(chlow).lineWise = (op == CommandOperation::CopyLines);
                }
                else
                {
                    GetEditor().GetRegister(registers.top()).text = str;
                    GetEditor().GetRegister(registers.top()).lineWise = (op == CommandOperation::CopyLines);
                }
                registers.pop();
            }
        }
    }

    // Handle command
    if (op == CommandOperation::Delete ||
        op == CommandOperation::DeleteLines)
    {
        auto cmd = std::make_shared<ZepCommand_DeleteRange>(*pBuffer,
            beginRange,
            endRange,
            cursorAfter != -1 ? cursorAfter : beginRange
            );
        commandResult.spCommand = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    }
    else if (op == CommandOperation::Insert && !pRegister->text.empty())
    {
        auto cmd = std::make_shared<ZepCommand_Insert>(*pBuffer,
            beginRange,
            pRegister->text,
            cursorAfter
            );
        commandResult.spCommand = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    }
    else if (op == CommandOperation::Copy ||
        op == CommandOperation::CopyLines)
    {
        // Copy commands may move the cursor
        if (cursorAfter != -1)
        {
            m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(cursorAfter));
        }
        return true;
    }

    return false;
}

// Parse the command into:
// [count1] opA [count2] opB
// And generate (count1 * count2), opAopB
std::string ZepMode_Vim::GetCommandAndCount(std::string strCommand, int& count)
{
    std::string count1;
    std::string command1;
    std::string count2;
    std::string command2;

    auto itr = strCommand.begin();
    while (itr != strCommand.end() && std::isdigit(*itr))
    {
        count1 += *itr;
        itr++;
    }

    while (itr != strCommand.end()
        && std::isgraph(*itr) && !std::isdigit(*itr))
    {
        command1 += *itr;
        itr++;
    }

    // If not a register target, then another count
    if (command1[0] != '\"' &&
        command1[0] != ':')
    {
        while (itr != strCommand.end()
            && std::isdigit(*itr))
        {
            count2 += *itr;
            itr++;
        }
    }

    while (itr != strCommand.end()
        && (std::isgraph(*itr) || *itr == ' '))
    {
        command2 += *itr;
        itr++;
    }

    bool foundCount = false;
    count = 1;
    if (!count1.empty())
    {
        count = std::stoi(count1);
        foundCount = true;
    }
    if (!count2.empty())
    {
        // When 2 counts are specified, they multiply!
        // Such as 2d2d, which deletes 4 lines
        count *= std::stoi(count2);
        foundCount = true;
    }

    // Concatentate the parts of the command into a single command string
    std::string command = command1 + command2;

    // Short circuit - 0 is special, first on line.  Since we don't want to confuse it 
    // with a command count
    if (count == 0)
    {
        count = 1;
        return "0";
    }

    // The dot command is like the 'last' command that succeeded
    if (command == ".")
    {
        command = m_lastCommand;
        count = foundCount ? count : m_lastCount;
    }
    return command;
}

void ZepMode_Vim::Enable()
{
    if (m_pCurrentWindow)
    {
        m_pCurrentWindow->SetCursorMode(CursorMode::Normal);
    }
    m_currentMode = EditorMode::Normal;
}

void ZepMode_Vim::AddKeyPress(uint32_t key, uint32_t modifierKeys)
{
    if (!m_pCurrentWindow)
        return;

    // Reset command text - we will update it later
    m_pCurrentWindow->GetDisplay().SetCommandText("");

    if (m_currentMode == EditorMode::Normal ||
        m_currentMode == EditorMode::Visual)
    {
        // Escape wins all
        if (key == ExtKeys::ESCAPE)
        {
            SwitchMode(EditorMode::Normal);
            return;
        }

        // Update the typed command
        m_currentCommand += char(key);

        // ... and show it in the command bar
        m_pCurrentWindow->GetDisplay().SetCommandText(m_currentCommand);

        // Retrieve the vim command
        int count;
        std::string command = GetCommandAndCount(m_currentCommand, count);

        CommandResult commandResult;
        if (GetCommand(command, key, modifierKeys, m_currentMode, count, commandResult))
        {
            // Remember a new modification command and clear the last dot command string
            if (commandResult.spCommand && key != '.')
            {
                m_lastCommand = command;
                m_lastCount = count;
                m_lastInsertString.clear();
            }

            // Dot group means we have an extra command to append
            // This is to make a command and insert into a single undo operation
            bool appendDotInsert = false;

            // Label group beginning
            if (commandResult.spCommand)
            {
                if (key == '.' && !m_lastInsertString.empty() && commandResult.modeSwitch == EditorMode::Insert)
                {
                    appendDotInsert = true;
                }
                
                if (appendDotInsert ||
                    (count > 1 &&
                    !(commandResult.flags & CommandResultFlags::HandledCount)))
                {
                    commandResult.spCommand->SetFlags(CommandFlags::GroupBoundary);
                }
                AddCommand(commandResult.spCommand);
            }

            // Next commands (for counts)
            if (!(commandResult.flags & CommandResultFlags::HandledCount))
            {
                for (int i = 1; i < count; i++)
                {
                    if (GetCommand(command, key, modifierKeys, m_currentMode, count, commandResult) &&
                        commandResult.spCommand)
                    {
                        // Group counted
                        if (i == (count - 1) && !appendDotInsert)
                        {
                            commandResult.spCommand->SetFlags(CommandFlags::GroupBoundary);
                        }

                        // Actually queue/do command
                        AddCommand(commandResult.spCommand);
                    }
                }
            }

            ResetCommand();
            
            // A mode to switch to after the command is done
            SwitchMode(commandResult.modeSwitch);

            // If used dot command, append the inserted text.  This is a little confusing.
            // TODO: Think of a cleaner way to express it
            if (appendDotInsert)
            {
                if (!m_lastInsertString.empty())
                {
                    auto cmd = std::make_shared<ZepCommand_Insert>(*m_pCurrentWindow->GetCurrentBuffer(),
                        m_pCurrentWindow->DisplayToBuffer(),
                        m_lastInsertString,
                        m_pCurrentWindow->GetCurrentBuffer()->LocationFromOffsetByChars(m_pCurrentWindow->DisplayToBuffer(), long(m_lastInsertString.size())));
                    cmd->SetFlags(CommandFlags::GroupBoundary);
                    AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
                }
                SwitchMode(EditorMode::Normal);
            }

            // Any motions while in Vim mode will update the selection
            UpdateVisualSelection();
        }

        // Make cursor visible right after command
        if (m_pCurrentWindow)
        {
            m_pCurrentWindow->GetDisplay().ResetCursorTimer();
        }
    }
    else if (m_currentMode == EditorMode::Insert)
    {
        HandleInsert(key);
        ResetCommand();
    }
}

void ZepMode_Vim::HandleInsert(uint32_t key)
{
    auto cursor = m_pCurrentWindow->GetCursor();

    // Operations outside of inserts will pack up the insert operation 
    // and start a new one
    bool packCommand = false;
    switch (key)
    {
    case ExtKeys::ESCAPE:
    case ExtKeys::BACKSPACE:
    case ExtKeys::DEL:
    case ExtKeys::RIGHT:
    case ExtKeys::LEFT:
    case ExtKeys::UP:
    case ExtKeys::DOWN:
        packCommand = true;
        break;
    default:
        break;
    }

    if (m_pendingEscape)
    {
        // My custom 'jk' escape option
        auto canEscape = m_spInsertEscapeTimer->GetDelta() < .25f;
        if (canEscape && key == 'k')
        {
            packCommand = true;
            key = ExtKeys::ESCAPE;
        }
        m_pendingEscape = false;
    }

    const auto bufferCursor = m_pCurrentWindow->DisplayToBuffer(cursor);
    auto pBuffer = m_pCurrentWindow->GetCurrentBuffer();

    // Escape back to normal mode
    if (packCommand)
    {
        // End location is where we just finished typing
        auto insertEnd = bufferCursor;
        if (insertEnd > m_insertBegin)
        {
            // Get the string we inserted
            auto strInserted = std::string(pBuffer->GetText().begin() + m_insertBegin, pBuffer->GetText().begin() + insertEnd);

            // Remember the inserted string for repeating the command
            m_lastInsertString = strInserted;

            // Temporarily remove it
            pBuffer->Delete(m_insertBegin, insertEnd);

            // Generate a command to put it in with undoable state
            // Leave cusor at the end
            auto cmd = std::make_shared<ZepCommand_Insert>(*pBuffer, m_insertBegin, strInserted, insertEnd);
            AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        }

        // Finished escaping
        if (key == ExtKeys::ESCAPE)
        {
            if (cursor.x != 0)
            {
                auto finalCursor = pBuffer->LocationFromOffsetByChars(insertEnd, -1);
                m_pCurrentWindow->MoveCursorTo(finalCursor);
            }

            // Back to normal mode
            SwitchMode(EditorMode::Normal);
        }
        else
        {
            // Any other key here is a command while in insert mode
            // For example, hitting Backspace
            // There is more work to do here to support keyboard combos in insert mode
            // (not that I can think of ones that I use!)
            CommandResult res;
            if (GetCommand("", key, 0, EditorMode::Insert, 1, res) &&
                res.spCommand)
            {
                AddCommand(res.spCommand);
            }
            SwitchMode(EditorMode::Insert);
        }
        return;
    }

    auto buf = bufferCursor;
    std::string ch((char*)&key);
    if (key == ExtKeys::RETURN)
    {
        ch = "\r\n";
    }
    else if (key == ExtKeys::TAB)
    {
        // 4 Spaces, obviously :)
        ch = "    ";
    }

    if (key == 'j' && !m_pendingEscape)
    {
        m_spInsertEscapeTimer->Restart();
        m_pendingEscape = true;
    }
    else
    {
        // If we thought it was an escape but it wasn't, put the 'j' back in!
        if (m_pendingEscape)
        {
            ch = "j" + ch;
        }
        m_pendingEscape = false;

        pBuffer->Insert(buf, ch);

        // Insert back to normal mode should put the cursor on top of the last character typed.
        auto newCursor = pBuffer->LocationFromOffset(buf, long(ch.size()));
        m_pCurrentWindow->MoveCursorTo(newCursor, LineLocation::LineCRBegin);
    }
}

void ZepMode_Vim::SetCurrentWindow(ZepWindow* pWindow)
{
    ZepMode::SetCurrentWindow(pWindow);

    // If we thought it was an escape but it wasn't, put the 'j' back in!
    // TODO: Move to a more sensible place where we can check the time
    if (m_pendingEscape &&
        m_spInsertEscapeTimer->GetDelta() > .25f)
    {
        m_pendingEscape = false;
        auto buf = pWindow->DisplayToBuffer();
        pWindow->m_pCurrentBuffer->Insert(buf, "j");
        pWindow->MoveCursor(NVec2i(1, 0), LineLocation::LineCRBegin);
    }
}
} // Zep
