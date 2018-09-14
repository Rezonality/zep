#include <cctype>
#include <sstream>

#include "buffer.h"
#include "commands.h"
#include "mode_vim.h"
#include "tab_window.h"
#include "utils/stringutils.h"
#include "utils/timer.h"
#include "window.h"

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
// CTRL+F/B/D/U page and have page moves
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

// TODO: I think a better implementation of the block commands might be a set of simple quiries about what the cursor is currently over
// and where the next thing is; a kind of state machine you can use to search for the next thing.  Not sure.  Certainly, the motions stuff
// is tricky to get right (hence all the unit tests!).  They all have their particular behaviors which are annoying if not correctly matching vim.
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

// Find the end of the first word we are on, or the end of the space we are on.
// TODO: Make all our motion helpers read like this!
BufferLocation ToEndOfFirstWordOrSpace(const BufferBlock& block)
{
    if (block.spaceBefore)
        return block.firstBlock;

    return block.firstNonBlock;
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

CommandContext::CommandContext(const std::string& commandIn,
    ZepMode_Vim& md,
    uint32_t lastK,
    uint32_t modifierK,
    EditorMode editorMode)
    : commandText(commandIn)
    , owner(md)
    , buffer(md.GetCurrentWindow()->GetBuffer())
    , cursor(md.GetCurrentWindow()->GetCursor())
    , bufferCursor(md.GetCurrentWindow()->DisplayToBuffer(cursor))
    , lastKey(lastK)
    , modifierKeys(modifierK)
    , mode(editorMode)
    , tempReg("", false)
{
    registers.push('"');
    pRegister = &tempReg;

    long displayLineCount = long(owner.GetCurrentWindow()->visibleLines.size());
    if (displayLineCount > cursor.y)
    {
        pLineInfo = &owner.GetCurrentWindow()->visibleLines[cursor.y];
    }

    GetCommandAndCount();
    GetCommandRegisters();
}

void CommandContext::UpdateRegisters()
{
    // Store in a register
    if (registers.empty())
        return;

    if (op == CommandOperation::Delete || op == CommandOperation::DeleteLines)
    {
        beginRange = std::max(beginRange, BufferLocation{ 0 });
        endRange = std::max(endRange, BufferLocation{ 0 });
        assert(beginRange <= endRange);
        if (beginRange > endRange)
        {
            beginRange = endRange;
        }
        std::string str = std::string(buffer.GetText().begin() + beginRange, buffer.GetText().begin() + endRange);

        // Delete commands fill up 1-9 registers
        if (command[0] == 'd' || command[0] == 'D')
        {
            for (int i = 9; i > 1; i--)
            {
                owner.GetEditor().SetRegister('0' + i, owner.GetEditor().GetRegister('0' + i - 1));
            }
            owner.GetEditor().SetRegister('1', Register(str, (op == CommandOperation::DeleteLines)));
        }

        // Fill up any other required registers
        while (!registers.empty())
        {
            owner.GetEditor().SetRegister(registers.top(), Register(str, (op == CommandOperation::DeleteLines)));
            registers.pop();
        }
    }
    else if (op == CommandOperation::Copy || op == CommandOperation::CopyLines)
    {
        std::string str = std::string(buffer.GetText().begin() + beginRange, buffer.GetText().begin() + endRange);
        while (!registers.empty())
        {
            // Capital letters append to registers instead of replacing them
            if (registers.top() >= 'A' && registers.top() <= 'Z')
            {
                auto chlow = std::tolower(registers.top());
                owner.GetEditor().GetRegister(chlow).text += str;
                owner.GetEditor().GetRegister(chlow).lineWise = (op == CommandOperation::CopyLines);
            }
            else
            {
                owner.GetEditor().GetRegister(registers.top()).text = str;
                owner.GetEditor().GetRegister(registers.top()).lineWise = (op == CommandOperation::CopyLines);
            }
            registers.pop();
        }
    }
}

// Parse the command into:
// [count1] opA [count2] opB
// And generate (count1 * count2), opAopB
void CommandContext::GetCommandAndCount()
{
    std::string count1;
    std::string command1;
    std::string count2;
    std::string command2;

    auto itr = commandText.begin();
    while (itr != commandText.end() && std::isdigit(*itr))
    {
        count1 += *itr;
        itr++;
    }

    while (itr != commandText.end()
        && std::isgraph(*itr) && !std::isdigit(*itr))
    {
        command1 += *itr;
        itr++;
    }

    // If not a register target, then another count
    if (command1[0] != '\"' && command1[0] != ':')
    {
        while (itr != commandText.end()
            && std::isdigit(*itr))
        {
            count2 += *itr;
            itr++;
        }
    }

    while (itr != commandText.end()
        && (std::isgraph(*itr) || *itr == ' '))
    {
        command2 += *itr;
        itr++;
    }

    bool foundCount = false;
    count = 1;

    try
    {
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
    }
    catch (std::out_of_range&)
    {
        // Ignore bad count
    }

    // Concatentate the parts of the command into a single command string
    commandWithoutCount = command1 + command2;

    // Short circuit - 0 is special, first on line.  Since we don't want to confuse it
    // with a command count
    if (count == 0)
    {
        count = 1;
        commandWithoutCount = "0";
    }

    // The dot command is like the 'last' command that succeeded
    if (commandWithoutCount == ".")
    {
        commandWithoutCount = owner.GetLastCommand();
        count = foundCount ? count : owner.GetLastCount();
    }
}

void CommandContext::GetCommandRegisters()
{
    // Store the register source
    if (commandWithoutCount[0] == '"' && commandWithoutCount.size() > 2)
    {
        // Null register
        if (commandWithoutCount[1] == '_')
        {
            std::stack<char> temp;
            registers.swap(temp);
        }
        else
        {
            registers.push(commandWithoutCount[1]);
            char reg = commandWithoutCount[1];

            // Demote capitals to lower registers when pasting (all both)
            if (reg >= 'A' && reg <= 'Z')
            {
                reg = std::tolower(reg);
            }

            if (owner.GetEditor().GetRegisters().find(std::string({ reg })) != owner.GetEditor().GetRegisters().end())
            {
                pRegister = &owner.GetEditor().GetRegister(reg);
            }
        }
        command = commandWithoutCount.substr(2);
    }
    else
    {
        // Default register
        if (pRegister->text.empty())
        {
            pRegister = &owner.GetEditor().GetRegister('"');
        }
        command = commandWithoutCount;
    }
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
    m_spInsertEscapeTimer = std::make_shared<Timer>();
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

    if (mode == EditorMode::Insert && m_pCurrentWindow && m_pCurrentWindow->GetBuffer().IsViewOnly())
    {
        mode = EditorMode::Normal;
    }

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
    auto& buffer = m_pCurrentWindow->GetBuffer();
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
            endRange = buffer.LocationFromOffsetByChars(m_visualEnd, 1);
            cursorAfter = beginRange;
        }
    }
    else if (op == "line")
    {
        // Whole line
        if (pLineInfo)
        {
            beginRange = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineBegin);
            endRange = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineEnd);
            cursorAfter = beginRange;
        }
    }
    else if (op == "$")
    {
        if (pLineInfo)
        {
            beginRange = bufferCursor;
            endRange = buffer.GetLinePos(pLineInfo->lineNumber, LineLocation::LineCRBegin);
            cursorAfter = beginRange;
        }
    }
    else if (op == "w")
    {
        auto block = buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = block.blockSearchPos;
        endRange = WordMotion(block);
        cursorAfter = beginRange;
    }
    else if (op == "cw")
    {
        // Change word doesn't extend over the next space
        auto block = buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = block.blockSearchPos;
        endRange = ToEndOfFirstWordOrSpace(block);
        cursorAfter = beginRange;
    }
    else if (op == "cW")
    {
        // Change word doesn't extend over the next space
        auto block = buffer.GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = block.blockSearchPos;
        endRange = ToEndOfFirstWordOrSpace(block);
        cursorAfter = beginRange;
    }
    else if (op == "W")
    {
        auto block = buffer.GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = block.blockSearchPos;
        endRange = WordMotion(block);
        cursorAfter = beginRange;
    }
    else if (op == "aw")
    {
        auto block = buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = Word(block).first;
        endRange = Word(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "aW")
    {
        auto block = buffer.GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = Word(block).first;
        endRange = Word(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "iw")
    {
        auto block = buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = InnerWord(block).first;
        endRange = InnerWord(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "iW")
    {
        auto block = buffer.GetBlock(SearchType::Word, bufferCursor, SearchDirection::Forward);
        beginRange = InnerWord(block).first;
        endRange = InnerWord(block).second;
        cursorAfter = beginRange;
    }
    else if (op == "cursor")
    {
        beginRange = bufferCursor;
        endRange = buffer.LocationFromOffsetByChars(bufferCursor, 1);
        cursorAfter = bufferCursor;
    }
    return beginRange != -1;
}

bool ZepMode_Vim::GetCommand(CommandContext& context)
{
    // Motion
    if (context.command == "$")
    {
        m_pCurrentWindow->MoveCursorTo(context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineCRBegin));
        return true;
    }
    else if (context.command == "0")
    {
        m_pCurrentWindow->MoveCursorTo(context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineBegin));
        return true;
    }
    else if (context.command == "^")
    {
        m_pCurrentWindow->MoveCursorTo(context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineFirstGraphChar));
        return true;
    }
    else if (context.command == "j" || context.command == "+" || context.lastKey == ExtKeys::DOWN)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, context.count));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "k" || context.command == "-" || context.lastKey == ExtKeys::UP)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, -context.count));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "l" || context.lastKey == ExtKeys::RIGHT)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(context.count, 0));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "h" || context.lastKey == ExtKeys::LEFT)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(-context.count, 0));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "f" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEDOWN)
    {
        // Note: the vim spec says 'visible lines - 2' for a 'page'.
        // We jump the max possible lines, which might hit the end of the text; this matches observed vim behavior
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, (m_pCurrentWindow->GetMaxDisplayLines() - 2) * context.count));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "d" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEDOWN)
    {
        // Note: the vim spec says 'half visible lines' for up/down
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, (context.displayLineCount / 2) * context.count));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "b" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEUP)
    {
        // Note: the vim spec says 'visible lines - 2' for a 'page'
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, -(m_pCurrentWindow->GetMaxDisplayLines() - 2) * context.count));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "u" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEUP)
    {
        m_pCurrentWindow->MoveCursor(Zep::NVec2i(0, -(context.displayLineCount / 2) * context.count));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "G")
    {
        if (context.count != 1)
        {
            // Goto line
            // TODO I don't think this will move beyond the displayed lines with a line count.
            m_pCurrentWindow->MoveCursorTo(context.buffer.GetLinePos(context.count, LineLocation::LineBegin));
            context.commandResult.flags |= CommandResultFlags::HandledCount;
        }
        else
        {
            // Move right to the end
            m_pCurrentWindow->MoveCursor(Zep::NVec2i(Zep::MaxCursorMove, Zep::MaxCursorMove));
            context.commandResult.flags |= CommandResultFlags::HandledCount;
        }
        return true;
    }
    else if (context.lastKey == ExtKeys::BACKSPACE)
    {
        auto loc = context.bufferCursor;

        // Insert-mode context.command
        if (context.mode == EditorMode::Insert)
        {
            // In insert mode, we are 'on' the character after the one we want to delete
            context.beginRange = context.buffer.LocationFromOffsetByChars(loc, -1);
            context.endRange = context.buffer.LocationFromOffsetByChars(loc, 0);
            context.cursorAfter = context.beginRange;
            context.op = CommandOperation::Delete;
        }
        else
        {
            // Normal mode moves over the chars, and wraps
            m_pCurrentWindow->MoveCursorTo(context.buffer.LocationFromOffsetByChars(loc, -1));
            return true;
        }
    }
    else if (context.command == "w")
    {
        auto block = context.buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, context.bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (context.command == "W")
    {
        auto block = context.buffer.GetBlock(SearchType::Word, context.bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (context.command == "b")
    {
        auto block = context.buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, context.bufferCursor, SearchDirection::Backward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (context.command == "B")
    {
        auto block = context.buffer.GetBlock(SearchType::Word, context.bufferCursor, SearchDirection::Backward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordMotion(block)));
        return true;
    }
    else if (context.command == "e")
    {
        auto block = context.buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, context.bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
        return true;
    }
    else if (context.command == "E")
    {
        auto block = context.buffer.GetBlock(SearchType::Word, context.bufferCursor, SearchDirection::Forward);
        m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
        return true;
    }
    else if (context.command[0] == 'g')
    {
        if (context.command == "g")
        {
            context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
        }
        else if (context.command == "ge")
        {
            auto block = context.buffer.GetBlock(SearchType::AlphaNumeric | SearchType::Word, context.bufferCursor, SearchDirection::Backward);
            m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
            return true;
        }
        else if (context.command == "gE")
        {
            auto block = context.buffer.GetBlock(SearchType::Word, context.bufferCursor, SearchDirection::Backward);
            m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(WordEndMotion(block)));
            return true;
        }
        else if (context.command == "gg")
        {
            m_pCurrentWindow->MoveCursorTo(BufferLocation{ 0 });
            return true;
        }
    }
    else if (context.command == "J")
    {
        // Delete the CR (and thus join lines)
        context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineCRBegin);
        context.endRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineEnd);
        context.cursorAfter = context.bufferCursor;
        context.op = CommandOperation::Delete;
    }
    else if (context.command == "v" || context.command == "V")
    {
        if (m_currentMode == EditorMode::Visual)
        {
            context.commandResult.modeSwitch = EditorMode::Normal;
        }
        else
        {
            if (context.command == "V")
            {
                m_visualBegin = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineBegin);
                m_visualEnd = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineEnd) - 1;
            }
            else
            {
                m_visualBegin = context.bufferCursor;
                m_visualEnd = m_visualBegin;
            }
            context.commandResult.modeSwitch = EditorMode::Visual;
        }
        m_lineWise = context.command == "V" ? true : false;
        return true;
    }
    else if (context.command == "x" || context.lastKey == ExtKeys::DEL)
    {
        auto loc = context.bufferCursor;

        if (m_currentMode == EditorMode::Visual)
        {
            context.beginRange = m_visualBegin;
            context.endRange = context.buffer.LocationFromOffsetByChars(m_visualEnd, 1);
            context.cursorAfter = m_visualBegin;
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Normal;
        }
        else
        {
            // Don't allow x to delete beyond the end of the line
            if (context.command != "x" || std::isgraph(context.buffer.GetText()[loc]) || std::isblank(context.buffer.GetText()[loc]))
            {
                context.beginRange = loc;
                context.endRange = context.buffer.LocationFromOffsetByChars(loc, 1);
                context.cursorAfter = loc;
                context.op = CommandOperation::Delete;
            }
            else
            {
                ResetCommand();
            }
        }
    }
    else if (context.command[0] == 'o')
    {
        context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineEnd);
        context.tempReg.text = "\n";
        context.pRegister = &context.tempReg;
        context.cursorAfter = context.beginRange;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (context.command[0] == 'O')
    {
        context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineBegin);
        context.tempReg.text = "\n";
        context.pRegister = &context.tempReg;
        context.cursorAfter = context.beginRange;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (context.command[0] == 'd' || context.command == "D")
    {
        if (context.command == "d")
        {
            // Only in visual mode; delete selected block
            if (GetBlockOpRange("visual", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
                context.commandResult.modeSwitch = EditorMode::Normal;
            }
            else
            {
                context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
            }
        }
        else if (context.command == "dd")
        {
            if (GetBlockOpRange("line", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::DeleteLines;
                context.commandResult.modeSwitch = EditorMode::Normal;
            }
        }
        else if (context.command == "d$" || context.command == "D")
        {
            if (GetBlockOpRange("$", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "dw")
        {
            if (GetBlockOpRange("w", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "dW")
        {
            if (GetBlockOpRange("W", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "da")
        {
            context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
        }
        else if (context.command == "daw")
        {
            if (GetBlockOpRange("aw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "daW")
        {
            if (GetBlockOpRange("aW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "di")
        {
            context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
        }
        else if (context.command == "diw")
        {
            if (GetBlockOpRange("iw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "diW")
        {
            if (GetBlockOpRange("iW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
    }
    // Substitute
    else if ((context.command[0] == 's') || (context.command[0] == 'S'))
    {
        if (context.command == "S")
        {
            if (context.pLineInfo)
            {
                // Delete whole line and go to insert mode
                context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineBegin);
                context.endRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineCRBegin);
                context.cursorAfter = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineFirstGraphChar);
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "s")
        {
            // Only in visual mode; delete selected block and go to insert mode
            if (GetBlockOpRange("visual", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
            // Just delete under cursor and insert
            else if (GetBlockOpRange("cursor", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else
        {
            return false;
        }
        context.commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (context.command[0] == 'C' || context.command[0] == 'c')
    {
        if (context.command == "c")
        {
            // Only in visual mode; delete selected block
            if (GetBlockOpRange("visual", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
            else
            {
                context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
            }
        }
        else if (context.command == "cc")
        {
            if (GetBlockOpRange("line", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::DeleteLines;
            }
        }
        else if (context.command == "c$" || context.command == "C")
        {
            if (GetBlockOpRange("$", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "cw")
        {
            if (GetBlockOpRange("cw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "cW")
        {
            if (GetBlockOpRange("cW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "ca")
        {
            context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
        }
        else if (context.command == "caw")
        {
            if (GetBlockOpRange("aw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "caW")
        {
            if (GetBlockOpRange("aW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "ci")
        {
            context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
        }
        else if (context.command == "ciw")
        {
            if (GetBlockOpRange("iw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "ciW")
        {
            if (GetBlockOpRange("iW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }

        if (context.op != CommandOperation::None)
        {
            context.commandResult.modeSwitch = EditorMode::Insert;
        }
    }
    else if (context.command == "p")
    {
        if (!context.pRegister->text.empty())
        {
            if (context.pRegister->lineWise)
            {
                context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineEnd);
                context.cursorAfter = context.beginRange;
            }
            else
            {
                context.beginRange = context.buffer.LocationFromOffsetByChars(context.bufferCursor, 1);
                context.cursorAfter = context.buffer.LocationFromOffset(context.beginRange, long(StringUtils::Utf8Length(context.pRegister->text.c_str())) - 1);
            }
            context.op = CommandOperation::Insert;
        }
    }
    else if (context.command == "P")
    {
        if (!context.pRegister->text.empty())
        {
            if (context.pRegister->lineWise)
            {
                context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineBegin);
                context.cursorAfter = context.beginRange;
            }
            else
            {
                context.beginRange = context.bufferCursor;
                context.cursorAfter = context.buffer.LocationFromOffsetByChars(context.beginRange, long(StringUtils::Utf8Length(context.pRegister->text.c_str()) - 1));
            }
            context.op = CommandOperation::Insert;
        }
    }
    else if (context.command[0] == 'y')
    {
        if (context.mode == EditorMode::Visual)
        {
            context.registers.push('0');
            context.beginRange = m_visualBegin;
            context.endRange = context.buffer.LocationFromOffsetByChars(m_visualEnd, 1);
            context.cursorAfter = m_visualBegin;
            context.commandResult.modeSwitch = EditorMode::Normal;
            context.op = m_lineWise ? CommandOperation::CopyLines : CommandOperation::Copy;
        }
        else if (context.mode == EditorMode::Normal)
        {
            if (context.command == "y")
            {
                context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
            }
            else if (context.command == "yy")
            {
                if (context.pLineInfo)
                {
                    // Copy the whole line, including the CR
                    context.registers.push('0');
                    context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineBegin);
                    context.endRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineEnd) + 1;
                    context.op = CommandOperation::CopyLines;
                }
            }
        }

        if (context.op == CommandOperation::None)
        {
            return false;
        }
    }
    else if (context.command == "Y")
    {
        if (context.pLineInfo)
        {
            // Copy the whole line, including the CR
            context.registers.push('0');
            context.beginRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineBegin);
            context.endRange = context.buffer.GetLinePos(context.pLineInfo->lineNumber, LineLocation::LineEnd);
            context.op = CommandOperation::CopyLines;
            context.commandResult.modeSwitch = EditorMode::Normal;
        }
    }
    else if (context.command == "u")
    {
        Undo();
        return true;
    }
    else if (context.command == "r" && context.modifierKeys == ModifierKey::Ctrl)
    {
        Redo();
        return true;
    }
    else if (context.command == "i")
    {
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == "a")
    {
        // Cursor append
        m_pCurrentWindow->MoveCursor(NVec2i(1, 0), LineLocation::LineCRBegin);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == "A")
    {
        // Cursor append to end of line
        m_pCurrentWindow->MoveCursor(LineLocation::LineCRBegin);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == "I")
    {
        // Cursor Insert beginning char of line
        m_pCurrentWindow->MoveCursor(LineLocation::LineFirstGraphChar);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.lastKey == ExtKeys::RETURN)
    {
        if (context.command[0] == ':')
        {
            if (GetEditor().Broadcast(std::make_shared<ZepMessage>(Msg_HandleCommand, context.command)))
            {
                return true;
            }
            else if (context.command == ":reg")
            {
                std::ostringstream str;
                str << "--- Registers ---" << '\n';
                for (auto& reg : GetEditor().GetRegisters())
                {
                    if (!reg.second.text.empty())
                    {
                        std::string displayText = reg.second.text;
                        displayText = StringUtils::ReplaceString(displayText, "\n", "^J");
                        str << "\"" << reg.first << "   " << displayText << '\n';
                    }
                }
                m_pCurrentWindow->GetDisplay().SetCommandText(str.str());
                return true;
            }
            else if (context.command == ":ls")
            {
                std::ostringstream str;
                str << "--- context.buffers ---" << '\n';
                int index = 0;
                for (auto& buffer : GetEditor().GetBuffers())
                {
                    if (!buffer->GetName().empty())
                    {
                        std::string displayText = buffer->GetName();
                        displayText = StringUtils::ReplaceString(displayText, "\n", "^J");
                        if (&m_pCurrentWindow->GetBuffer() == buffer.get())
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
            else if (context.command.find(":bu") == 0)
            {
                auto strTok = StringUtils::Split(context.command, " ");

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
                                m_pCurrentWindow->GetTabWindow().SetCurrentBuffer(buffer.get());
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
            else
            {
                m_pCurrentWindow->GetDisplay().SetCommandText("Not a context.command");
                return false;
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

    // Update the registers based on context state
    context.UpdateRegisters();

    // Setup command, if any
    if (context.op == CommandOperation::Delete || context.op == CommandOperation::DeleteLines)
    {
        auto cmd = std::make_shared<ZepCommand_DeleteRange>(context.buffer,
            context.beginRange,
            context.endRange,
            context.cursorAfter != -1 ? context.cursorAfter : context.beginRange);
        context.commandResult.spCommand = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    }
    else if (context.op == CommandOperation::Insert && !context.pRegister->text.empty())
    {
        auto cmd = std::make_shared<ZepCommand_Insert>(context.buffer,
            context.beginRange,
            context.pRegister->text,
            context.cursorAfter);
        context.commandResult.spCommand = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    }
    else if (context.op == CommandOperation::Copy || context.op == CommandOperation::CopyLines)
    {
        // Copy commands may move the cursor
        if (context.cursorAfter != -1)
        {
            m_pCurrentWindow->SetCursor(m_pCurrentWindow->BufferToDisplay(context.cursorAfter));
        }
        return true;
    }

    return false;
}

void ZepMode_Vim::Begin()
{
    if (m_pCurrentWindow)
    {
        m_pCurrentWindow->SetCursorMode(CursorMode::Normal);
        m_pCurrentWindow->GetDisplay().SetCommandText(m_currentCommand);
    }
    m_currentMode = EditorMode::Normal;
    m_currentCommand.clear();
    m_lastCommand.clear();
    m_lastCount = 0;
    m_pendingEscape = false;
}

void ZepMode_Vim::AddKeyPress(uint32_t key, uint32_t modifierKeys)
{
    if (!m_pCurrentWindow)
        return;

    // Reset command text - we will update it later
    m_pCurrentWindow->GetDisplay().SetCommandText("");

    if (m_currentMode == EditorMode::Normal || m_currentMode == EditorMode::Visual)
    {
        // Escape wins all
        if (key == ExtKeys::ESCAPE)
        {
            SwitchMode(EditorMode::Normal);
            return;
        }

        // Update the typed command
        m_currentCommand += char(key);

        // ... and show it in the command bar if desired
        if (m_currentCommand[0] == ':' || m_settings.ShowNormalModeKeyStrokes)
        {
            m_pCurrentWindow->GetDisplay().SetCommandText(m_currentCommand);
        }

        CommandContext context(m_currentCommand, *this, key, modifierKeys, m_currentMode);

        if (GetCommand(context))
        {
            // Remember a new modification command and clear the last dot command string
            if (context.commandResult.spCommand && key != '.')
            {
                m_lastCommand = context.command;
                m_lastCount = context.count;
                m_lastInsertString.clear();
            }

            // Dot group means we have an extra command to append
            // This is to make a command and insert into a single undo operation
            bool appendDotInsert = false;

            // Label group beginning
            if (context.commandResult.spCommand)
            {
                if (key == '.' && !m_lastInsertString.empty() && context.commandResult.modeSwitch == EditorMode::Insert)
                {
                    appendDotInsert = true;
                }

                if (appendDotInsert || (context.count > 1 && !(context.commandResult.flags & CommandResultFlags::HandledCount)))
                {
                    context.commandResult.spCommand->SetFlags(CommandFlags::GroupBoundary);
                }
                AddCommand(context.commandResult.spCommand);
            }

            // Next commands (for counts)
            // Many command handlers do the right thing for counts; if they don't we basically interpret the command
            // multiple times to implement it.
            if (!(context.commandResult.flags & CommandResultFlags::HandledCount))
            {
                for (int i = 1; i < context.count; i++)
                {
                    // May immediate execute and not return a command...
                    // Create a new 'inner' context for the next command, because we need to re-initialize the command
                    // context for 'after' what just happened!
                    CommandContext contextInner(m_currentCommand, *this, key, modifierKeys, m_currentMode);
                    if (GetCommand(contextInner) && contextInner.commandResult.spCommand)
                    {
                        // Group counted
                        if (i == (context.count - 1) && !appendDotInsert)
                        {
                            contextInner.commandResult.spCommand->SetFlags(CommandFlags::GroupBoundary);
                        }

                        // Actually queue/do command
                        AddCommand(contextInner.commandResult.spCommand);
                    }
                }
            }

            ResetCommand();

            // A mode to switch to after the command is done
            SwitchMode(context.commandResult.modeSwitch);

            // If used dot command, append the inserted text.  This is a little confusing.
            // TODO: Think of a cleaner way to express it
            if (appendDotInsert)
            {
                if (!m_lastInsertString.empty())
                {
                    auto cmd = std::make_shared<ZepCommand_Insert>(m_pCurrentWindow->GetBuffer(),
                        m_pCurrentWindow->DisplayToBuffer(),
                        m_lastInsertString,
                        m_pCurrentWindow->GetBuffer().LocationFromOffsetByChars(m_pCurrentWindow->DisplayToBuffer(), long(m_lastInsertString.size())));
                    cmd->SetFlags(CommandFlags::GroupBoundary);
                    AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
                }
                SwitchMode(EditorMode::Normal);
            }

            // Any motions while in Vim mode will update the selection
            UpdateVisualSelection();
        }
        else
        {
            if (!m_currentCommand.empty() && m_currentCommand.find_first_not_of("0123456789") == std::string::npos)
            {
                context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
            }
            // Handled, but no new command

            // A better mechanism is required for clearing pending commands!
            if (m_currentCommand[0] != ':' && m_currentCommand[0] != '"' && !(context.commandResult.flags & CommandResultFlags::NeedMoreChars))
            {
                ResetCommand();
            }
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
    case ExtKeys::PAGEUP:
    case ExtKeys::PAGEDOWN:
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
    auto& buffer = m_pCurrentWindow->GetBuffer();

    // Escape back to normal mode
    if (packCommand)
    {
        // End location is where we just finished typing
        auto insertEnd = bufferCursor;
        if (insertEnd > m_insertBegin)
        {
            // Get the string we inserted
            auto strInserted = std::string(buffer.GetText().begin() + m_insertBegin, buffer.GetText().begin() + insertEnd);

            // Remember the inserted string for repeating the command
            m_lastInsertString = strInserted;

            // Temporarily remove it
            buffer.Delete(m_insertBegin, insertEnd);

            // Generate a command to put it in with undoable state
            // Leave cusor at the end
            auto cmd = std::make_shared<ZepCommand_Insert>(buffer, m_insertBegin, strInserted, insertEnd);
            AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        }

        // Finished escaping
        if (key == ExtKeys::ESCAPE)
        {
            if (cursor.x != 0)
            {
                auto finalCursor = buffer.LocationFromOffsetByChars(insertEnd, -1);
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
            CommandContext context("", *this, key, 0, EditorMode::Insert);
            if (GetCommand(context) && context.commandResult.spCommand)
            {
                AddCommand(context.commandResult.spCommand);
            }
            SwitchMode(EditorMode::Insert);
        }
        return;
    }

    auto buf = bufferCursor;
    std::string ch((char*)&key);
    if (key == ExtKeys::RETURN)
    {
        ch = "\n";
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

        buffer.Insert(buf, ch);

        // Insert back to normal mode should put the cursor on top of the last character typed.
        auto newCursor = buffer.LocationFromOffset(buf, long(ch.size()));
        m_pCurrentWindow->MoveCursorTo(newCursor, LineLocation::LineCRBegin);
    }
}

void ZepMode_Vim::SetCurrentWindow(ZepWindow* pWindow)
{
    ZepMode::SetCurrentWindow(pWindow);

    // If we thought it was an escape but it wasn't, put the 'j' back in!
    // TODO: Move to a more sensible place where we can check the time
    if (m_pendingEscape && m_spInsertEscapeTimer->GetDelta() > .25f)
    {
        m_pendingEscape = false;
        auto buf = m_pCurrentWindow->DisplayToBuffer();
        m_pCurrentWindow->GetBuffer().Insert(buf, "j");
        m_pCurrentWindow->MoveCursor(NVec2i(1, 0), LineLocation::LineCRBegin);
    }
}
} // namespace Zep
