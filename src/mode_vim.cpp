#include <cctype>
#include <sstream>

#include "buffer.h"
#include "commands.h"
#include "mode_vim.h"
#include "mode_search.h"
#include "tab_window.h"
#include "mcommon/string/stringutils.h"
#include "mcommon/animation/timer.h"
#include "window.h"
#include "theme.h"

// Note:
// This is a very basic implementation of the common Vim commands that I use: the bare minimum I can live with.
// I do use more, and depending on how much pain I suffer, will add them over time.
// My aim is to make it easy to add commands, so if you want to put something in, please send me a PR.
// The buffer/display search and find support makes it easy to gather the info you need, and the basic insert/delete undo redo commands
// make it easy to find the locations in the buffer
// Important to note: I'm not trying to beat/better Vim here.  Just make an editor I can use in a viewport without feeling pain!
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
// di[({})]"'
// c[a]<count>w/e  Change word
// ci[({})]"'

namespace Zep
{

CommandContext::CommandContext(const std::string& commandIn, ZepMode_Vim& md, uint32_t lastK, uint32_t modifierK, EditorMode editorMode)
    : owner(md)
    , commandText(commandIn)
    , buffer(md.GetCurrentWindow()->GetBuffer())
    , bufferCursor(md.GetCurrentWindow()->GetBufferCursor())
    , tempReg("", false)
    , lastKey(lastK)
    , modifierKeys(modifierK)
    , mode(editorMode)
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
        beginRange = std::max(beginRange, BufferLocation{0});
        endRange = std::max(endRange, BufferLocation{0});
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
                owner.GetEditor().SetRegister('0' + (char)i, owner.GetEditor().GetRegister('0' + (char)i - 1));
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
        beginRange = std::max(beginRange, BufferLocation{0});
        endRange = std::max(endRange, BufferLocation{0});
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
                owner.GetEditor().GetRegister((const char)chlow).text += str;
                owner.GetEditor().GetRegister((const char)chlow).lineWise = (op == CommandOperation::CopyLines);
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

    // Special case; 'f3' is a find for the character '3', not a count of 3!
    // But 2f3 is 'find the second 3'....
    if (itr != commandText.end())
    {
        if (*itr == 'f' || *itr == 'F' || *itr == 'c')
        {
            while (itr != commandText.end()
                   && std::isgraph(*itr))
            {
                command1 += *itr;
                itr++;
            }
        }
        else
        {
            while (itr != commandText.end()
                   && std::isgraph(*itr) && !std::isdigit(*itr))
            {
                command1 += *itr;
                itr++;
            }
        }
    }

    // If not a register target, then another count
    if (command1[0] != '\"' && command1[0] != ':' && command1[0] != '/')
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
                reg = (char)std::tolower((char)reg);
            }

            if (owner.GetEditor().GetRegisters().find(std::string({reg})) != owner.GetEditor().GetRegisters().end())
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
    timer_restart(m_insertEscapeTimer);
    for (int i = 0; i <= 9; i++)
    {
        GetEditor().SetRegister('0' + (const char)i, "");
    }
    GetEditor().SetRegister('"', "");
}

void ZepMode_Vim::ResetCommand()
{
    m_currentCommand.clear();
}

void ZepMode_Vim::ClampCursorForMode()
{
    // Normal mode cursor is never on a CR/0
    if (m_currentMode == EditorMode::Normal)
    {
        GetCurrentWindow()->SetBufferCursor(GetCurrentWindow()->GetBuffer().ClampToVisibleLine(GetCurrentWindow()->GetBufferCursor()));
    }
}

void ZepMode_Vim::SwitchMode(EditorMode mode)
{
    // Don't switch to invalid mode
    if (mode == EditorMode::None)
        return;

    if (mode == EditorMode::Insert && GetCurrentWindow() && GetCurrentWindow()->GetBuffer().TestFlags(FileFlags::Locked))
    {
        mode = EditorMode::Normal;
    }

    m_currentMode = mode;
    switch (mode)
    {
        case EditorMode::Normal:
        {
            GetCurrentWindow()->SetCursorType(CursorType::Normal);
            ClampCursorForMode();
            ResetCommand();
        }
        break;
        case EditorMode::Insert:
            m_insertBegin = GetCurrentWindow()->GetBufferCursor();
            GetCurrentWindow()->SetCursorType(CursorType::Insert);
            m_pendingEscape = false;
            break;
        case EditorMode::Visual:
            GetCurrentWindow()->SetCursorType(CursorType::Visual);
            ResetCommand();
            m_pendingEscape = false;
            break;
        default:
        case EditorMode::None:
            break;
    }
}

bool ZepMode_Vim::GetOperationRange(const std::string& op, EditorMode mode, BufferLocation& beginRange, BufferLocation& endRange) const
{
    auto& buffer = GetCurrentWindow()->GetBuffer();
    const auto bufferCursor = GetCurrentWindow()->GetBufferCursor();

    beginRange = BufferLocation{-1};
    if (op == "visual")
    {
        if (mode == EditorMode::Visual)
        {
            beginRange = m_visualBegin;
            endRange = m_visualEnd;
        }
    }
    else if (op == "line")
    {
        // Whole line
        beginRange = buffer.GetLinePos(bufferCursor, LineLocation::LineBegin);
        endRange = buffer.GetLinePos(bufferCursor, LineLocation::BeyondLineEnd);
    }
    else if (op == "$")
    {
        beginRange = bufferCursor;
        endRange = buffer.GetLinePos(bufferCursor, LineLocation::LineCRBegin);
    }
    else if (op == "w")
    {
        beginRange = bufferCursor;
        endRange = buffer.WordMotion(bufferCursor, SearchType::Word, SearchDirection::Forward);
    }
    else if (op == "cw")
    {
        // Change word doesn't extend over the next space
        beginRange = bufferCursor;
        endRange = buffer.ChangeWordMotion(bufferCursor, SearchType::Word, SearchDirection::Forward);
    }
    else if (op == "cW")
    {
        beginRange = bufferCursor;
        endRange = buffer.ChangeWordMotion(bufferCursor, SearchType::WORD, SearchDirection::Forward);
    }
    else if (op == "W")
    {
        beginRange = bufferCursor;
        endRange = buffer.WordMotion(bufferCursor, SearchType::WORD, SearchDirection::Forward);
    }
    else if (op == "aw")
    {
        auto range = buffer.AWordMotion(bufferCursor, SearchType::Word);
        beginRange = range.first;
        endRange = range.second;
    }
    else if (op == "aW")
    {
        auto range = buffer.AWordMotion(bufferCursor, SearchType::WORD);
        beginRange = range.first;
        endRange = range.second;
    }
    else if (op == "iw")
    {
        auto range = buffer.InnerWordMotion(bufferCursor, SearchType::Word);
        beginRange = range.first;
        endRange = range.second;
    }
    else if (op == "iW")
    {
        auto range = buffer.InnerWordMotion(bufferCursor, SearchType::WORD);
        beginRange = range.first;
        endRange = range.second;
    }
    else if (op == "cursor")
    {
        beginRange = bufferCursor;
        endRange = buffer.LocationFromOffsetByChars(bufferCursor, 1);
    }
    return beginRange != -1;
}

bool ZepMode_Vim::HandleExCommand(const std::string& strCommand, const char key)
{
    if (key == ExtKeys::BACKSPACE && !strCommand.empty())
    {
        m_currentCommand.pop_back();
    }
    else
    {
        if (std::isgraph(key) || std::isblank(key))
        {
            m_currentCommand += char(key);
        }
    }

    if (key == ExtKeys::RETURN)
    {
        auto pWindow = GetEditor().GetActiveTabWindow()->GetActiveWindow();
        auto& buffer = pWindow->GetBuffer();
        auto bufferCursor = pWindow->GetBufferCursor();
        if (GetEditor().Broadcast(std::make_shared<ZepMessage>(Msg::HandleCommand, strCommand)))
        {
            m_currentCommand.clear();
            return true;
        }
        else if (strCommand == ":reg")
        {
            std::ostringstream str;
            str << "--- Registers ---" << '\n';
            for (auto& reg : GetEditor().GetRegisters())
            {
                if (!reg.second.text.empty())
                {
                    std::string displayText = reg.second.text;
                    displayText = string_replace(displayText, "\n", "^J");
                    str << "\"" << reg.first << "   " << displayText << '\n';
                }
            }
            GetEditor().SetCommandText(str.str());
        }
        else if (strCommand.find(":tabedit") == 0)
        {
            auto pTab = GetEditor().AddTabWindow();
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1)
            {
                if (strTok[1] == "%")
                {
                    pTab->AddWindow(&GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer(), nullptr, true);
                }
                else
                {
                    auto fname = strTok[1];
                    auto pBuffer = GetEditor().GetFileBuffer(fname);
                    pTab->AddWindow(pBuffer, nullptr, true);
                }
            }
            else
            {
                pTab->AddWindow(GetEditor().GetEmptyBuffer("Empty"), nullptr, true);
            }
            GetEditor().SetCurrentTabWindow(pTab);
        }
        else if (strCommand.find(":vsplit") == 0)
        {
            auto pTab = GetEditor().GetActiveTabWindow();
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1)
            {
                if (strTok[1] == "%")
                {
                    pTab->AddWindow(&GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer(), pWindow, true);
                }
                else
                {
                    auto fname = strTok[1];
                    auto pBuffer = GetEditor().GetFileBuffer(fname);
                    pTab->AddWindow(pBuffer, pWindow, true);
                }
            }
            else
            {
                pTab->AddWindow(&GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer(), pWindow, true);
            }
        }
        else if (strCommand.find(":hsplit") == 0 || strCommand.find(":split") == 0)
        {
            auto pTab = GetEditor().GetActiveTabWindow();
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1)
            {
                if (strTok[1] == "%")
                {
                    pTab->AddWindow(&GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer(), pWindow, false);
                }
                else
                {
                    auto fname = strTok[1];
                    auto pBuffer = GetEditor().GetFileBuffer(fname);
                    pTab->AddWindow(pBuffer, pWindow, false);
                }
            }
            else
            {
                pTab->AddWindow(&GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer(), pWindow, false);
            }
        }
        else if (strCommand.find(":e") == 0)
        {
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1)
            {
                auto fname = strTok[1];
                auto pBuffer = GetEditor().GetFileBuffer(fname);
                pWindow->SetBuffer(pBuffer);
            }
        }
        else if (strCommand.find(":w") == 0)
        {
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1)
            {
                auto fname = strTok[1];
                GetCurrentWindow()->GetBuffer().SetFilePath(fname);
            }
            GetEditor().SaveBuffer(pWindow->GetBuffer());
        }
        else if (strCommand == ":close" || strCommand == ":clo")
        {
            GetEditor().GetActiveTabWindow()->CloseActiveWindow();
        }
        else if (strCommand[1] == 'q')
        {
            if (strCommand == ":q")
            {
                GetEditor().GetActiveTabWindow()->CloseActiveWindow();
            }
        }
        else if (strCommand.find(":ZTestMarkers") == 0)
        {
            int markerType = 0;
            auto strTok = string_split(strCommand, " ");
            if (strTok.size() > 1)
            {
                markerType = std::stoi(strTok[1]);
            }
            auto spMarker = std::make_shared<RangeMarker>();
            long start, end;

            if (m_currentMode == EditorMode::Visual)
            {
                auto range = GetCurrentWindow()->GetBuffer().GetSelection();
                start = range.first;
                end = range.second;
            }
            else
            {
                start = buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar);
                end = buffer.GetLinePos(bufferCursor, LineLocation::LineLastGraphChar) + 1;
            }
            spMarker->range = BufferRange{start, end};
            switch (markerType)
            {
                case 3:
                    spMarker->highlightColor = ThemeColor::TabActive;
                    spMarker->textColor = ThemeColor::Text;
                    spMarker->name = "Filled Marker";
                    spMarker->description = "This is an example tooltip\nThey can be added to any range of characters";
                    spMarker->displayType = RangeMarkerDisplayType::Tooltip | RangeMarkerDisplayType::Underline | RangeMarkerDisplayType::Indicator;
                    break;
                case 2:
                    spMarker->highlightColor = ThemeColor::Warning;
                    spMarker->textColor = ThemeColor::Text;
                    spMarker->name = "Tooltip";
                    spMarker->description = "This is an example tooltip\nThey can be added to any range of characters";
                    spMarker->displayType = RangeMarkerDisplayType::Tooltip;
                    break;
                case 1:
                    spMarker->highlightColor = ThemeColor::Warning;
                    spMarker->textColor = ThemeColor::Text;
                    spMarker->name = "Warning";
                    spMarker->description = "This is an example warning mark";
                    break;
                case 0:
                default:
                    spMarker->highlightColor = ThemeColor::Error;
                    spMarker->textColor = ThemeColor::Text;
                    spMarker->name = "Error";
                    spMarker->description = "This is an example error mark";
            }
            buffer.AddRangeMarker(spMarker);
        }
        else if (strCommand == ":ZWhiteSpace")
        {
            pWindow->ToggleFlag(WindowFlags::ShowCR);
        }
        else if (strCommand == ":ZThemeToggle")
        {
            // An easy test command to check per-buffer themeing
            // Just gets the current theme on the buffer and makes a new
            // one that is opposite
            auto theme = pWindow->GetBuffer().GetTheme();
            auto spNewTheme = std::make_shared<ZepTheme>();
            if (theme.GetThemeType() == ThemeType::Dark)
            {
                spNewTheme->SetThemeType(ThemeType::Light);
            }
            else
            {
                spNewTheme->SetThemeType(ThemeType::Dark);
            }
            pWindow->GetBuffer().SetTheme(spNewTheme);
        }
        else if (strCommand == ":ls")
        {
            std::ostringstream str;
            str << "--- buffers ---" << '\n';
            int index = 0;
            for (auto& editor_buffer : GetEditor().GetBuffers())
            {
                if (!editor_buffer->GetName().empty())
                {
                    std::string displayText = editor_buffer->GetName();
                    displayText = string_replace(displayText, "\n", "^J");
                    if (&GetCurrentWindow()->GetBuffer() == editor_buffer.get())
                    {
                        str << "*";
                    }
                    else
                    {
                        str << " ";
                    }
                    if (editor_buffer->TestFlags(FileFlags::Dirty))
                    {
                        str << "+";
                    }
                    else
                    {
                        str << " ";
                    }
                    str << index++ << " : " << displayText << '\n';
                }
            }
            GetEditor().SetCommandText(str.str());
        }
        else if (strCommand.find(":bu") == 0)
        {
            auto strTok = string_split(strCommand, " ");

            if (strTok.size() > 1)
            {
                try
                {
                    auto index = std::stoi(strTok[1]);
                    auto current = 0;
                    for (auto& editor_buffer : GetEditor().GetBuffers())
                    {
                        if (index == current)
                        {
                            GetCurrentWindow()->SetBuffer(editor_buffer.get());
                        }
                        current++;
                    }
                }
                catch (std::exception&)
                {
                }
            }
        }
        else
        {
            GetEditor().SetCommandText("Not a command");
        }

        m_currentCommand.clear();
        return true;
    }
    return false;
}

bool ZepMode_Vim::GetCommand(CommandContext& context)
{
    auto bufferCursor = GetCurrentWindow()->GetBufferCursor();
    auto& buffer = GetCurrentWindow()->GetBuffer();

    // CTRL + keys common to modes
    bool needMoreChars = false;
    if ((context.modifierKeys & ModifierKey::Ctrl) &&
        HandleGlobalCtrlCommand(context.command, context.modifierKeys, needMoreChars))
    {
        if (needMoreChars)
        {
            context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
            return false;
        }
        else
        {
            return true;
        }
    }
    // Motion
    else if (context.command == "$")
    {
        GetCurrentWindow()->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineLastNonCR));
        return true;
    }
    else if (context.command == "0")
    {
        GetCurrentWindow()->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineBegin));
        return true;
    }
    else if (context.command == "^")
    {
        GetCurrentWindow()->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar));
        return true;
    }
    // Moving between tabs
    else if (context.command == "H" && (context.modifierKeys & ModifierKey::Shift))
    {
        GetEditor().PreviousTabWindow();
        return true;
    }
    else if (context.command == "L" && (context.modifierKeys & ModifierKey::Shift))
    {
        GetEditor().NextTabWindow();
        return true;
    }
    else if (context.command == "j" || context.command == "+" || context.lastKey == ExtKeys::DOWN)
    {
        GetCurrentWindow()->MoveCursorY(context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "k" || context.command == "-" || context.lastKey == ExtKeys::UP)
    {
        GetCurrentWindow()->MoveCursorY(-context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "l" || context.lastKey == ExtKeys::RIGHT)
    {
        auto lineEnd = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineLastNonCR);
        GetCurrentWindow()->SetBufferCursor(std::min(context.bufferCursor + context.count, lineEnd));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "h" || context.lastKey == ExtKeys::LEFT)
    {
        auto lineStart = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
        GetCurrentWindow()->SetBufferCursor(std::max(context.bufferCursor - context.count, lineStart));
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "f" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEDOWN)
    {
        // Note: the vim spec says 'visible lines - 2' for a 'page'.
        // We jump the max possible lines, which might hit the end of the text; this matches observed vim behavior
        GetCurrentWindow()->MoveCursorY((GetCurrentWindow()->GetMaxDisplayLines() - 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "d" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEDOWN)
    {
        // Note: the vim spec says 'half visible lines' for up/down
        GetCurrentWindow()->MoveCursorY((GetCurrentWindow()->GetNumDisplayedLines() / 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "b" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEUP)
    {
        // Note: the vim spec says 'visible lines - 2' for a 'page'
        GetCurrentWindow()->MoveCursorY(-(GetCurrentWindow()->GetMaxDisplayLines() - 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if ((context.command == "u" && (context.modifierKeys & ModifierKey::Ctrl)) || context.lastKey == ExtKeys::PAGEUP)
    {
        GetCurrentWindow()->MoveCursorY(-(GetCurrentWindow()->GetNumDisplayedLines() / 2) * context.count);
        context.commandResult.flags |= CommandResultFlags::HandledCount;
        return true;
    }
    else if (context.command == "G")
    {
        if (context.foundCount)
        {
            // In Vim, 0G means go to end!  1G is the first line...
            long count = context.count - 1;
            count = std::min(context.buffer.GetLineCount() - 1, count);
            if (count < 0)
                count = context.buffer.GetLineCount() - 1;

            long start, end;
            if (context.buffer.GetLineOffsets(count, start, end))
            {
                GetCurrentWindow()->SetBufferCursor(start);
            }
        }
        else
        {
            // Move right to the end
            auto lastLine = context.buffer.GetLinePos(MaxCursorMove, LineLocation::LineBegin);
            GetCurrentWindow()->SetBufferCursor(lastLine);
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
            context.op = CommandOperation::Delete;
        }
        else
        {
            // Normal mode moves over the chars, and wraps
            GetCurrentWindow()->SetBufferCursor(context.bufferCursor - 1);
            return true;
        }
    }
    else if (context.command == "w")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::Word, SearchDirection::Forward);
        GetCurrentWindow()->SetBufferCursor(target);
        return true;
    }
    else if (context.command == "W")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Forward);
        GetCurrentWindow()->SetBufferCursor(target);
        return true;
    }
    else if (context.command == "b")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::Word, SearchDirection::Backward);
        GetCurrentWindow()->SetBufferCursor(target);
        return true;
    }
    else if (context.command == "B")
    {
        auto target = context.buffer.WordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Backward);
        GetCurrentWindow()->SetBufferCursor(target);
        return true;
    }
    else if (context.command == "e")
    {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::Word, SearchDirection::Forward);
        GetCurrentWindow()->SetBufferCursor(target);
        return true;
    }
    else if (context.command == "E")
    {
        auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Forward);
        GetCurrentWindow()->SetBufferCursor(target);
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
            GetCurrentWindow()->SetBufferCursor(target);
            return true;
        }
        else if (context.command == "gE")
        {
            auto target = context.buffer.EndWordMotion(context.bufferCursor, SearchType::WORD, SearchDirection::Backward);
            GetCurrentWindow()->SetBufferCursor(target);
            return true;
        }
        else if (context.command == "gg")
        {
            GetCurrentWindow()->SetBufferCursor(BufferLocation{0});
            return true;
        }
    }
    else if (context.command == "J")
    {
        // Delete the CR (and thus join lines)
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineCRBegin);
        context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
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
                m_visualEnd = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
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
            context.endRange = m_visualEnd;
            context.op = CommandOperation::Delete;
            context.commandResult.modeSwitch = EditorMode::Normal;
        }
        else
        {
            // Don't allow x to delete beyond the end of the line
            if (context.command != "x" || std::isgraph(context.buffer.GetText()[loc]) || std::isblank(context.buffer.GetText()[loc]))
            {
                context.beginRange = loc;
                context.endRange = std::min(context.buffer.GetLinePos(loc, LineLocation::LineCRBegin),
                                            context.buffer.LocationFromOffsetByChars(loc, context.count));
                context.op = CommandOperation::Delete;
                context.commandResult.flags |= CommandResultFlags::HandledCount;
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
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
    }
    else if (context.command[0] == 'O')
    {
        context.beginRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::LineBegin);
        context.tempReg.text = "\n";
        context.pRegister = &context.tempReg;
        context.op = CommandOperation::Insert;
        context.commandResult.modeSwitch = EditorMode::Insert;
        context.cursorAfterOverride = context.bufferCursor;
    }
    else if (context.command[0] == 'd' || context.command == "D")
    {
        if (context.command == "d")
        {
            // Only in visual mode; delete selected block
            if (GetOperationRange("visual", context.mode, context.beginRange, context.endRange))
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
            if (GetOperationRange("line", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::DeleteLines;
                context.commandResult.modeSwitch = EditorMode::Normal;
            }
        }
        else if (context.command == "d$" || context.command == "D")
        {
            if (GetOperationRange("$", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "dw")
        {
            if (GetOperationRange("w", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "dW")
        {
            if (GetOperationRange("W", context.mode, context.beginRange, context.endRange))
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
            if (GetOperationRange("aw", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "daW")
        {
            if (GetOperationRange("aW", context.mode, context.beginRange, context.endRange))
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
            if (GetOperationRange("iw", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "diW")
        {
            if (GetOperationRange("iW", context.mode, context.beginRange, context.endRange))
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
            context.op = CommandOperation::Delete;
        }
        else if (context.command == "s")
        {
            // Only in visual mode; delete selected block and go to insert mode
            if (GetOperationRange("visual", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
            // Just delete under m_bufferCursor and insert
            else if (GetOperationRange("cursor", context.mode, context.beginRange, context.endRange))
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
            if (GetOperationRange("visual", context.mode, context.beginRange, context.endRange))
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
            if (GetOperationRange("line", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::DeleteLines;
            }
        }
        else if (context.command == "c$" || context.command == "C")
        {
            if (GetOperationRange("$", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "cw")
        {
            if (GetOperationRange("cw", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "cW")
        {
            if (GetOperationRange("cW", context.mode, context.beginRange, context.endRange))
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
            if (GetOperationRange("aw", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "caW")
        {
            if (GetOperationRange("aW", context.mode, context.beginRange, context.endRange))
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
            if (GetOperationRange("iw", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command == "ciW")
        {
            if (GetOperationRange("iW", context.mode, context.beginRange, context.endRange))
            {
                context.op = CommandOperation::Delete;
            }
        }
        else if (context.command.find("ct") == 0)
        {
            if (context.command.length() == 3)
            {
                context.beginRange = bufferCursor;
                context.endRange = buffer.FindOnLineMotion(bufferCursor, (const utf8*)&context.command[2], SearchDirection::Forward);
                context.op = CommandOperation::Delete;
            }
            else
            {
                context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
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
                context.cursorAfterOverride = context.beginRange;
            }
            else
            {
                context.beginRange = context.buffer.LocationFromOffsetByChars(context.bufferCursor, 1, LineLocation::LineCRBegin);
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
            }
            else
            {
                context.beginRange = context.bufferCursor;
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
            context.endRange = m_visualEnd;
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
                context.endRange = context.buffer.GetLinePos(context.bufferCursor, LineLocation::BeyondLineEnd);
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
        GetCurrentWindow()->SetBufferCursor(context.bufferCursor + 1);
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == "A")
    {
        // Cursor append to end of line
        GetCurrentWindow()->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineCRBegin));
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == "I")
    {
        // Cursor Insert beginning char of line
        GetCurrentWindow()->SetBufferCursor(context.buffer.GetLinePos(bufferCursor, LineLocation::LineFirstGraphChar));
        context.commandResult.modeSwitch = EditorMode::Insert;
        return true;
    }
    else if (context.command == ";")
    {
        if (!m_lastFind.empty())
        {
            GetCurrentWindow()->SetBufferCursor(context.buffer.FindOnLineMotion(bufferCursor, (const utf8*)m_lastFind.c_str(), m_lastFindDirection));
            return true;
        }
    }
    else if (context.command[0] == 'f')
    {
        if (context.command.length() > 1)
        {
            GetCurrentWindow()->SetBufferCursor(context.buffer.FindOnLineMotion(bufferCursor, (const utf8*)&context.command[1], SearchDirection::Forward));
            m_lastFind = context.command[1];
            m_lastFindDirection = SearchDirection::Forward;
            return true;
        }
        context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
    }
    else if (context.command[0] == 'F')
    {
        if (context.command.length() > 1)
        {
            GetCurrentWindow()->SetBufferCursor(context.buffer.FindOnLineMotion(bufferCursor, (const utf8*)&context.command[1], SearchDirection::Backward));
            m_lastFind = context.command[1];
            m_lastFindDirection = SearchDirection::Backward;
            return true;
        }
        context.commandResult.flags |= CommandResultFlags::NeedMoreChars;
    }
    else if (context.lastKey == ExtKeys::RETURN)
    {
        if (context.mode == EditorMode::Normal)
        {
            // Normal mode - RETURN moves cursor down a line!
            GetCurrentWindow()->MoveCursorY(1);
            GetCurrentWindow()->SetBufferCursor(context.buffer.GetLinePos(GetCurrentWindow()->GetBufferCursor(), LineLocation::LineBegin));
            return true;
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
        auto cmd = std::make_shared<ZepCommand_DeleteRange>(
            context.buffer,
            context.beginRange,
            context.endRange,
            context.bufferCursor);
        context.commandResult.spCommand = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    }
    else if (context.op == CommandOperation::Insert && !context.pRegister->text.empty())
    {
        auto cmd = std::make_shared<ZepCommand_Insert>(
            context.buffer,
            context.beginRange,
            context.pRegister->text,
            context.bufferCursor,
            context.cursorAfterOverride);
        context.commandResult.spCommand = std::static_pointer_cast<ZepCommand>(cmd);
        return true;
    }
    else if (context.op == CommandOperation::Copy || context.op == CommandOperation::CopyLines)
    {
        // Copy commands keep the cursor at the beginning
        GetCurrentWindow()->SetBufferCursor(context.beginRange);
        return true;
    }

    return false;
}

void ZepMode_Vim::Begin()
{
    if (GetCurrentWindow())
    {
        GetCurrentWindow()->SetCursorType(CursorType::Normal);
        GetEditor().SetCommandText(m_currentCommand);
    }
    m_currentMode = EditorMode::Normal;
    m_currentCommand.clear();
    m_lastCommand.clear();
    m_lastCount = 0;
    m_pendingEscape = false;
}

void ZepMode_Vim::UpdateVisualSelection()
{
    // Visual mode update - after a command
    if (m_currentMode == EditorMode::Visual)
    {
        // Update the visual range
        if (m_lineWise)
        {
            m_visualEnd = GetCurrentWindow()->GetBuffer().GetLinePos(GetCurrentWindow()->GetBufferCursor(), LineLocation::BeyondLineEnd);
        }
        else
        {
            m_visualEnd = GetCurrentWindow()->GetBuffer().LocationFromOffsetByChars(GetCurrentWindow()->GetBufferCursor(), 1);
        }
        GetCurrentWindow()->GetBuffer().SetSelection(BufferRange{m_visualBegin, m_visualEnd});
    }
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
        // TODO: Cursor keys on the command line
        if (key == ':' || m_currentCommand[0] == ':' || key == '/' || m_currentCommand[0] == '/')
        {
            if (HandleExCommand(m_currentCommand, (const char)key))
            {
                if (GetCurrentWindow())
                {
                    GetEditor().ResetCursorTimer();
                }
                return;
            }
        }

        // ... and show it in the command bar if desired
        if (m_currentCommand[0] == ':' || m_currentCommand[0] == '/' || m_settings.ShowNormalModeKeyStrokes)
        {
            GetEditor().SetCommandText(m_currentCommand);
            return;
        }

        m_currentCommand += char(key);

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
                    auto cmd = std::make_shared<ZepCommand_Insert>(
                        GetCurrentWindow()->GetBuffer(),
                        GetCurrentWindow()->GetBufferCursor(),
                        m_lastInsertString,
                        context.bufferCursor);
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

    ClampCursorForMode();
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
        auto canEscape = timer_get_elapsed_seconds(m_insertEscapeTimer) < .25f;
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

            auto lineBegin = buffer.GetLinePos(bufferCursor, LineLocation::LineBegin);

            // Temporarily remove it
            buffer.Delete(m_insertBegin, insertEnd);

            // Generate a command to put it in with undoable state
            // Leave cusor at the end of the insert, on the last inserted char.
            // ...but clamp to the line begin, so that a RETURN + ESCAPE lands you at the beginning of the new line
            auto cursorAfterEscape = std::max(lineBegin, m_insertBegin + BufferLocation(strInserted.length() - 1));

            auto cmd = std::make_shared<ZepCommand_Insert>(buffer, m_insertBegin, strInserted, m_insertBegin, cursorAfterEscape);
            AddCommand(std::static_pointer_cast<ZepCommand>(cmd));
        }

        // Finished escaping
        if (key == ExtKeys::ESCAPE)
        {
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
            context.bufferCursor = bufferCursor;
            if (GetCommand(context) && context.commandResult.spCommand)
            {
                AddCommand(context.commandResult.spCommand);
            }
            SwitchMode(EditorMode::Insert);
        }
        return;
    }

    std::string ch(1, (char)key);
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
        timer_restart(m_insertEscapeTimer);
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
        GetCurrentWindow()->SetBufferCursor(bufferCursor + long(ch.size()));
    }
}

void ZepMode_Vim::PreDisplay()
{

    // If we thought it was an escape but it wasn't, put the 'j' back in!
    // TODO: Move to a more sensible place where we can check the time
    if (m_pendingEscape && timer_get_elapsed_seconds(m_insertEscapeTimer) > .25f)
    {
        m_pendingEscape = false;
        GetCurrentWindow()->GetBuffer().Insert(GetCurrentWindow()->GetBufferCursor(), "j");
        GetCurrentWindow()->SetBufferCursor(GetCurrentWindow()->GetBufferCursor() + 1);
    }
}
} // namespace Zep
