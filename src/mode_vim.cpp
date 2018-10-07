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
// w,W,e,E,ge,gE,b,B WORD motions
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

namespace Zep
{

CommandContext::CommandContext(const std::string& commandIn,
    ZepMode_Vim& md,
    uint32_t lastK,
    uint32_t modifierK,
    EditorMode editorMode)
    : commandText(commandIn)
    , owner(md)
    , buffer(md.GetCurrentWindow()->GetBuffer())
    , bufferCursor(md.GetCurrentWindow()->GetBufferCursor())
    , lastKey(lastK)
    , modifierKeys(modifierK)
    , mode(editorMode)
    , tempReg("", false)
{
    registers.push('"');
    pRegister = &tempReg;

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
        if (beginRange > endRange)
        {
            std::swap(beginRange, endRange);
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
        beginRange = std::max(beginRange, BufferLocation{ 0 });
        endRange = std::max(endRange, BufferLocation{ 0 });
        if (beginRange > endRange)
        {
            std::swap(beginRange, endRange);
        }

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

    foundCount = false;
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

    if (mode == EditorMode::Insert && GetCurrentWindow() && GetCurrentWindow()->GetBuffer().IsViewOnly())
    {
        mode = EditorMode::Normal;
    }

    m_currentMode = mode;
    switch (mode)
    {
    case EditorMode::Normal:
        GetCurrentWindow()->SetCursorMode(CursorMode::Normal);
        ResetCommand();
        break;
    case EditorMode::Insert:
        m_insertBegin = GetCurrentWindow()->GetBufferCursor();
        GetCurrentWindow()->SetCursorMode(CursorMode::Insert);
        m_pendingEscape = false;
        break;
    case EditorMode::Visual:
        GetCurrentWindow()->SetCursorMode(CursorMode::Visual);
        ResetCommand();
        m_pendingEscape = false;
        break;
    default:
    case EditorMode::Command:
    case EditorMode::None:
        break;
    }
}

bool ZepMode_Vim::GetOperationRange(const std::string& op, EditorMode mode, BufferLocation& beginRange, BufferLocation& endRange, BufferLocation& cursorAfter) const
{
    auto& buffer = GetCurrentWindow()->GetBuffer();
    const auto bufferCursor = GetCurrentWindow()->GetBufferCursor();

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
        beginRange = buffer.GetLinePos(bufferCursor, LineLocation::LineBegin);
        endRange = buffer.GetLinePos(bufferCursor, LineLocation::BeyondLineEnd);
        cursorAfter = beginRange;
    }
    else if (op == "$")
    {
        beginRange = bufferCursor;
        endRange = buffer.GetLinePos(bufferCursor, LineLocation::LineCRBegin);
        cursorAfter = beginRange;
    }
    else if (op == "w")
    {
        beginRange = bufferCursor;
        endRange = buffer.WordMotion(bufferCursor, SearchType::Word, SearchDirection::Forward);
        cursorAfter = beginRange;
    }
    else if (op == "cw")
    {
        // Change word doesn't extend over the next space
        beginRange = bufferCursor;
        endRange = buffer.ChangeWordMotion(bufferCursor, SearchType::Word, SearchDirection::Forward);
        cursorAfter = beginRange;
    }
    else if (op == "cW")
    {
        beginRange = bufferCursor;
        endRange = buffer.ChangeWordMotion(bufferCursor, SearchType::WORD, SearchDirection::Forward);
        cursorAfter = beginRange;
    }
    else if (op == "W")
    {
        beginRange = bufferCursor;
        endRange = buffer.WordMotion(bufferCursor, SearchType::WORD, SearchDirection::Forward);
        cursorAfter = beginRange;
    }
    else if (op == "aw")
    {
        auto range = buffer.AWordMotion(bufferCursor, SearchType::Word);
        beginRange = range.first;
        cursorAfter = range.first;
        endRange = range.second;
    }
    else if (op == "aW")
    {
        auto range = buffer.AWordMotion(bufferCursor, SearchType::WORD);
        beginRange = range.first;
        cursorAfter = range.first;
        endRange = range.second;
    }
    else if (op == "iw")
    {
        auto range = buffer.InnerWordMotion(bufferCursor, SearchType::Word);
        beginRange = range.first;
        cursorAfter = range.first;
        endRange = range.second;
    }
    else if (op == "iW")
    {
        auto range = buffer.InnerWordMotion(bufferCursor, SearchType::WORD);
        beginRange = range.first;
        cursorAfter = range.first;
        endRange = range.second;
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
        GetCurrentWindow()->MoveCursorInsideLine(LineLocation::LineLastNonCR);
        return true;
    }
    else if (context.command == "0")
    {
        GetCurrentWindow()->MoveCursorInsideLine(LineLocation::LineBegin);
        return true;
    }
    else if (context.command == "^")
    {
        GetCurrentWindow()->MoveCursorInsideLine(LineLocation::LineFirstGraphChar);
        return true;
    }
    else if (context.command == "j" || context.command == "+" || context.lastKey == ExtKeys::DOWN)
    {
        GetCurrentWindow()->MoveCursorWindowRelative(context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "k" || context.command == "-" || context.lastKey == ExtKeys::UP)
    {
        GetCurrentWindow()->MoveCursorWindowRelative(-context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "l" || context.lastKey == ExtKeys::RIGHT)
    {
        auto lineEnd = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineLastNonCR);
        GetCurrentWindow()->MoveCursorTo(std::min(context.bufferCursor + context.count, lineEnd));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "h" || context.lastKey == ExtKeys::LEFT)
    {
        auto lineStart = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
        GetCurrentWindow()->MoveCursorTo(std::max(context.bufferCursor - context.count, lineStart));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "f" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEDOWN)
    {
        // Note: the vim spec says 'visible lines - 2' for a 'page'.
        // We jump the max possible lines, which might hit the end of the text; this matches observed vim behavior
        GetCurrentWindow()->MoveCursorWindowRelative((GetCurrentWindow()->GetMaxDisplayLines() - 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "d" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEDOWN)
    {
        // Note: the vim spec says 'half visible lines' for up/down
        GetCurrentWindow()->MoveCursorWindowRelative((context.displayLineCount / 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "b" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEUP)
    {
        // Note: the vim spec says 'visible lines - 2' for a 'page'
        GetCurrentWindow()->MoveCursorWindowRelative(-(GetCurrentWindow()->GetMaxDisplayLines() - 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "u" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEUP)
    {
        GetCurrentWindow()->MoveCursorWindowRelative(-(context.displayLineCount / 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "G")
    {
        if (context.foundCount)
        {
            auto cursor = GetCurrentWindow()->BufferToDisplay();
            // TODO: This is incorrect; go to line in buffer
            if (cursor.x != -1)
            {
                // Goto line
                GetCurrentWindow()->MoveCursorWindowRelative(context.count - GetCurrentWindow()->GetCursorLineInfo(cursor.y).bufferLineNumber);
                context.commandResult.flags |= CommandResultFlags::HandledCount;
            }
        }
        else
        {
            // Move right to the end
            auto lastLine = context.buffer.GetLinePos(MaxCursorMove, LineLocation::LineBegin);
            GetCurrentWindow()->MoveCursorTo(lastLine);
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
            GetCurrentWindow()->MoveCursorTo(context.bufferCursor - 1);
            return true;
        }
    }
    else if (context.command == "w")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::Word, SearchDirection::Forward);
        GetCurrentWindow()->MoveCursorTo(target);
        return true;
    }
    else if (context.command == "W")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Forward);
        GetCurrentWindow()->MoveCursorTo(target);
        return true;
    }
    else if (context.command == "b")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::Word, SearchDirection::Backward);
        GetCurrentWindow()->MoveCursorTo(target);
        return true;
    }
    else if (context.command == "B")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Backward);
        GetCurrentWindow()->MoveCursorTo(target);
        return true;
    }
    else if (context.command == "e")
    {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::Word, SearchDirection::Forward);
        GetCurrentWindow()->MoveCursorTo(target);
        return true;
    }
    else if (context.command == "E")
    {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Forward);
        GetCurrentWindow()->MoveCursorTo(target);
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
            auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::Word, SearchDirection::Backward);
            GetCurrentWindow()->MoveCursorTo(target);
            return true;
        }
        else if (context.command == "gE")
        {
            auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Backward);
            GetCurrentWindow()->MoveCursorTo(target);
            return true;
        }
        else if (context.command == "gg")
        {
            GetCurrentWindow()->MoveCursorTo(BufferLocation{ 0 });
            return true;
        }
    }
    else if (context.command == "J")
    {
        // Delete the CR (and thus join lines)
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
        context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
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
                m_visualBegin = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
                m_visualEnd = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd) - 1;
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
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
        context.tempReg.text = "\n";
        context.pRegister = &context.tempReg;
        context.cursorAfter = context.beginRange + 1;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (context.command[0] == 'O')
    {
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
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
            if (GetOperationRange("visual", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            if (GetOperationRange("line", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::DeleteLines;
                context.commandResult.modeSwitch = EditorMode::Normal;
            }
        }
        else if (context.command == "d$" || context.command == "D")
        {
            if (GetOperationRange("$", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "dw")
        {
            if (GetOperationRange("w", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "dW")
        {
            if (GetOperationRange("W", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            if (GetOperationRange("aw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "daW")
        {
            if (GetOperationRange("aW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            if (GetOperationRange("iw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "diW")
        {
            if (GetOperationRange("iW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            // Delete whole line and go to insert mode
            context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
            context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
            context.cursorAfter = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineFirstGraphChar);
            context.op = CommandOperation::Delete;
        }
        else if (context.command == "s")
        {
            // Only in visual mode; delete selected block and go to insert mode
            if (GetOperationRange("visual", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
            // Just delete under m_bufferCursor and insert
            else if (GetOperationRange("cursor", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            if (GetOperationRange("visual", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            if (GetOperationRange("line", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::DeleteLines;
            }
        }
        else if (context.command == "c$" || context.command == "C")
        {
            if (GetOperationRange("$", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "cw")
        {
            if (GetOperationRange("cw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "cW")
        {
            if (GetOperationRange("cW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            if (GetOperationRange("aw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "caW")
        {
            if (GetOperationRange("aW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
            if (GetOperationRange("iw", context.mode, context.beginRange, context.endRange, context.cursorAfter))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "ciW")
        {
            if (GetOperationRange("iW", context.mode, context.beginRange, context.endRange, context.cursorAfter))
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
                context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
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
                context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
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
                // Copy the whole line, including the CR
                context.registers.push('0');
                context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
                context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd) + 1;
                context.op = CommandOperation::CopyLines;
            }
        }

        if (context.op == CommandOperation::None)
        {
            return false;
        }
    }
    else if (context.command == "Y")
    {
        // Copy the whole line, including the CR
        context.registers.push('0');
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
        context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
        context.op = CommandOperation::CopyLines;
        context.commandResult.modeSwitch = EditorMode::Normal;
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
        GetCurrentWindow()->MoveCursorTo(context.bufferCursor + 1);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == "A")
    {
        // Cursor append to end of line
        GetCurrentWindow()->MoveCursorInsideLine(LineLocation::LineCRBegin);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == "I")
    {
        // Cursor Insert beginning char of line
        GetCurrentWindow()->MoveCursorInsideLine(LineLocation::LineFirstGraphChar);
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
                GetEditor().SetCommandText(str.str());
                return true;
            }
            else if (context.command == ":tabedit")
            {
                auto pTab = GetEditor().AddTabWindow();
                pTab->AddWindow(&GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer());
            }
            else if (context.command == ":ls")
            {
                std::ostringstream str;
                str << "--- buffers ---" << '\n';
                int index = 0;
                for (auto& buffer : GetEditor().GetBuffers())
                {
                    if (!buffer->GetName().empty())
                    {
                        std::string displayText = buffer->GetName();
                        displayText = StringUtils::ReplaceString(displayText, "\n", "^J");
                        if (&GetCurrentWindow()->GetBuffer() == buffer.get())
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
                GetEditor().SetCommandText(str.str());
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
                                GetCurrentWindow()->SetBuffer(buffer.get());
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
                GetEditor().SetCommandText("Not a command");
                m_currentCommand.clear();
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
        // Copy commands may move the m_bufferCursor
        if (context.cursorAfter != -1)
        {
            GetCurrentWindow()->MoveCursorTo(context.cursorAfter);
        }
        return true;
    }

    return false;
}

void ZepMode_Vim::Begin()
{
    if (GetCurrentWindow())
    {
        GetCurrentWindow()->SetCursorMode(CursorMode::Normal);
        GetEditor().SetCommandText(m_currentCommand);
    }
    m_currentMode = EditorMode::Normal;
    m_currentCommand.clear();
    m_lastCommand.clear();
    m_lastCount = 0;
    m_pendingEscape = false;
}

void ZepMode_Vim::AddKeyPress(uint32_t key, uint32_t modifierKeys)
{
    if (!GetCurrentWindow())
        return;

    // Reset command text - we will update it later
    GetEditor().SetCommandText("");

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
            GetEditor().SetCommandText(m_currentCommand);
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
                    auto cmd = std::make_shared<ZepCommand_Insert>(GetCurrentWindow()->GetBuffer(),
                        GetCurrentWindow()->GetBufferCursor(),
                        m_lastInsertString,
                        GetCurrentWindow()->GetBuffer().LocationFromOffsetByChars(GetCurrentWindow()->GetBufferCursor(), long(m_lastInsertString.size())));
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

        // Make m_bufferCursor visible right after command
        if (GetCurrentWindow())
        {
            GetEditor().ResetCursorTimer();
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
    auto bufferCursor = GetCurrentWindow()->GetBufferCursor();

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

    auto& buffer = GetCurrentWindow()->GetBuffer();

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

            // Complete the insert on the last inserted character (otherwise, we end on the same place we started)
            insertEnd--;
        }

        // Finished escaping
        if (key == ExtKeys::ESCAPE)
        {
            if (bufferCursor != 0)
            {
                GetCurrentWindow()->MoveCursorTo(insertEnd);
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

        buffer.Insert(bufferCursor, ch);

        // Insert back to normal mode should put the m_bufferCursor on top of the last character typed.
        GetCurrentWindow()->MoveCursorTo(bufferCursor + long(ch.size()));
    }
}

void ZepMode_Vim::PreDisplay()
{

    // If we thought it was an escape but it wasn't, put the 'j' back in!
    // TODO: Move to a more sensible place where we can check the time
    if (m_pendingEscape && m_spInsertEscapeTimer->GetDelta() > .25f)
    {
        m_pendingEscape = false;
        GetCurrentWindow()->GetBuffer().Insert(GetCurrentWindow()->GetBufferCursor(), "j");
        GetCurrentWindow()->MoveCursorTo(GetCurrentWindow()->GetBufferCursor() + 1);
    }
}
} // namespace Zep
