#include <cctype>
#include <sstream>

#include "zep/keymap.h"
#include "zep/mode_search.h"
#include "zep/mode_vim.h"
#include "zep/tab_window.h"
#include "zep/theme.h"

#include "zep/mcommon/animation/timer.h"
#include "zep/mcommon/logger.h"
#include "zep/mcommon/string/stringutils.h"

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
// r Replace with char
// '$'
// 'yy'
// cc
// c$  Change to end of line
// C
// S, s, with visual modes
// ^
// 'O', 'o'
// 'V' (linewise v)
// Y, D, linewise yank/paste
// d[a]<count>w/e  Delete words
// di[({})]"'
// c[a]<count>w/e  Change word
// ci[({})]"'
// ct[char]/dt[char] Change to and delete to
// vi[Ww], va[Ww] Visual inner and word selections
// f[char] find on line
// /[string] find in file, 'n' find next

namespace Zep
{

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

    // Setup keymaps
    // Later, we will be able to read these from a file.
    // But keymaps are useful for overriding behavior in modes too
    auto keymap_vim = [](const std::vector<KeyMap*>& maps, const std::vector<std::string>& commands, const StringId& id) {
        for (auto& m : maps)
        {
            for (auto& c : commands)
            {
                keymap_add({ m }, { "<D><R>" + c }, id);
                keymap_add({ m }, { "<R>" + c }, id);
                keymap_add({ m }, { "<D>" + c }, id);
                keymap_add({ m }, { c }, id);
            }
        }
    };

    // Normal and Visual
    keymap_vim({ &m_normalMap, &m_visualMap }, { "Y" }, id_YankLine);
    keymap_vim({ &m_normalMap }, { "yy" }, id_YankLine);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "p" }, id_PasteAfter);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "P" }, id_PasteBefore);

    keymap_vim({ &m_normalMap, &m_visualMap }, { "x", "<Del>" }, id_Delete);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "J" }, id_JoinLines);

    // Motions

    // Line Motions
    keymap_vim({ &m_normalMap, &m_visualMap }, { "$" }, id_MotionLineEnd);
    keymap_add({ &m_normalMap, &m_visualMap }, { "0" }, id_MotionLineBegin);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "^" }, id_MotionLineFirstChar);

    // Page Motinos
    keymap_vim({ &m_normalMap, &m_visualMap }, { "j", "<Down>" }, id_MotionDown);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "k", "<Up>" }, id_MotionUp);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "l", "<Right>" }, id_MotionRight);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "h", "<Left>", "<Backspace>" }, id_MotionLeft);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "<C-f>", "<PageDown>" }, id_MotionPageForward);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "<C-b>", "<PageUp>" }, id_MotionPageBackward);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "<C-d>" }, id_MotionHalfPageForward);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "<C-u>" }, id_MotionHalfPageBackward);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "G" }, id_MotionGotoLine);

    // Word motions
    keymap_vim({ &m_normalMap, &m_visualMap }, { "w" }, id_MotionWord);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "b" }, id_MotionBackWord);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "W" }, id_MotionWORD);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "B" }, id_MotionBackWORD);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "e" }, id_MotionEndWord);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "E" }, id_MotionEndWORD);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "ge" }, id_MotionBackEndWord);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "gE" }, id_MotionBackEndWORD);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "gg" }, id_MotionGotoBeginning);

    keymap_vim({ &m_visualMap }, { "C" }, id_ChangeLine);
    keymap_vim({ &m_visualMap }, { "y" }, id_Yank);

    // Not necessary?
    keymap_vim({ &m_normalMap, &m_visualMap }, { "<Escape>" }, id_NormalMode);


    // Visual mode
    keymap_vim({ &m_visualMap }, { "aW" }, id_VisualSelectAWORD);
    keymap_vim({ &m_visualMap }, { "aw" }, id_VisualSelectAWord);
    keymap_vim({ &m_visualMap }, { "iW" }, id_VisualSelectInnerWORD);
    keymap_vim({ &m_visualMap }, { "iw" }, id_VisualSelectInnerWord);
    keymap_vim({ &m_visualMap }, { "d" }, id_VisualDelete);
    keymap_vim({ &m_visualMap }, { "c" }, id_VisualChange);
    keymap_vim({ &m_visualMap }, { "s" }, id_VisualSubstitute);

    // Normal mode only
    keymap_vim({ &m_normalMap }, { "i" }, id_InsertMode);
    keymap_vim({ &m_normalMap }, { "H" }, id_PreviousTabWindow);
    keymap_vim({ &m_normalMap }, { "L" }, id_NextTabWindow);
    keymap_vim({ &m_normalMap }, { "o" }, id_OpenLineBelow);
    keymap_vim({ &m_normalMap }, { "O" }, id_OpenLineAbove);
    keymap_vim({ &m_normalMap }, { "V" }, id_VisualLineMode);
    keymap_vim({ &m_normalMap }, { "v" }, id_VisualMode);

    keymap_vim({ &m_normalMap }, { "d<D>w", "dw" }, id_DeleteWord);
    keymap_vim({ &m_normalMap }, { "dW" }, id_DeleteWORD);
    keymap_vim({ &m_normalMap }, { "daw" }, id_DeleteAWord);
    keymap_vim({ &m_normalMap }, { "daW" }, id_DeleteAWORD);
    keymap_vim({ &m_normalMap }, { "diw" }, id_DeleteInnerWord);
    keymap_vim({ &m_normalMap }, { "diW" }, id_DeleteInnerWORD);
    keymap_vim({ &m_normalMap }, { "D", "d$" }, id_DeleteToLineEnd);
    keymap_vim({ &m_normalMap }, { "d<D>d", "dd" }, id_DeleteLine);
    keymap_vim({ &m_normalMap }, { "dt<.>" }, id_DeleteToChar);

    keymap_vim({ &m_normalMap }, { "cw" }, id_ChangeWord);
    keymap_vim({ &m_normalMap }, { "cW" }, id_ChangeWORD);
    keymap_vim({ &m_normalMap }, { "ciw" }, id_ChangeInnerWord);
    keymap_vim({ &m_normalMap }, { "ciW" }, id_ChangeInnerWORD);
    keymap_vim({ &m_normalMap }, { "caw" }, id_ChangeAWord);
    keymap_vim({ &m_normalMap }, { "caW" }, id_ChangeAWORD);
    keymap_vim({ &m_normalMap }, { "C", "c$" }, id_ChangeToLineEnd);
    keymap_vim({ &m_normalMap }, { "cc" }, id_ChangeLine);
    
    keymap_vim({ &m_normalMap }, { "ct<.>" }, id_ChangeToChar);

    keymap_vim({ &m_normalMap, &m_visualMap }, { "r<.>" }, id_Replace);

    keymap_vim({ &m_normalMap }, { "S" }, id_SubstituteLine);
    keymap_vim({ &m_normalMap }, { "s" }, id_Substitute);
    keymap_vim({ &m_normalMap }, { "A" }, id_AppendToLine);
    keymap_vim({ &m_normalMap }, { "a" }, id_Append);
    keymap_vim({ &m_normalMap }, { "I" }, id_InsertAtFirstChar);
    keymap_vim({ &m_normalMap }, { ";" }, id_FindNext);

    keymap_vim({ &m_normalMap }, { "n" }, id_MotionNextMarker);
    keymap_vim({ &m_normalMap }, { "N" }, id_MotionPreviousMarker);
    keymap_vim({ &m_normalMap }, { "<F8>" }, id_MotionNextMarker);
    keymap_vim({ &m_normalMap }, { "<S-F8>" }, id_MotionPreviousMarker);

    keymap_vim({ &m_normalMap }, { "+" }, id_FontBigger);
    keymap_vim({ &m_normalMap }, { "-" }, id_FontSmaller);

    keymap_vim({ &m_normalMap }, { "<C-i><C-o>" }, id_SwitchToAlternateFile);

    keymap_vim({ &m_normalMap }, { "<C-j>" }, id_MotionDownSplit);
    keymap_vim({ &m_normalMap }, { "<C-l>" }, id_MotionRightSplit);
    keymap_vim({ &m_normalMap }, { "<C-k>" }, id_MotionUpSplit);
    keymap_vim({ &m_normalMap }, { "<C-h>" }, id_MotionLeftSplit);
    keymap_vim({ &m_normalMap }, { "<Return>" }, id_MotionNextFirstChar);

    keymap_vim({ &m_normalMap }, { "<C-p>", "<C-,>" }, id_QuickSearch);
    keymap_vim({ &m_normalMap }, { "<C-r>" }, id_Redo);
    keymap_vim({ &m_normalMap }, { "<C-z>", "u" }, id_Undo);

    keymap_vim({ &m_normalMap, &m_visualMap }, { "f<.>" }, id_Find);
    keymap_vim({ &m_normalMap, &m_visualMap }, { "F<.>" }, id_FindBackwards);

    keymap_vim({ &m_normalMap, &m_visualMap }, { ":", "/", "?" }, id_ExMode);

    // Insert Mode
    keymap_add({ &m_insertMap }, { "<Backspace>" }, id_Backspace);
    keymap_add({ &m_insertMap }, { "<Return>" }, id_InsertCarriageReturn);
    keymap_add({ &m_insertMap }, { "<Tab>" }, id_InsertTab);
    keymap_add({ &m_insertMap }, { "jk" }, id_NormalMode);
    keymap_add({ &m_insertMap }, { "<Escape>" }, id_NormalMode);

    /*
    // These searches are for substrings...
    // Counts are any digit sequences in the command
    // Match any digit; but not as the very last character
    std::regex countGroup(R"((\d+)\D+)"); 

    // Match any register; currently ascii or _ 
    std::regex registerGroup(R"(("\w))");
   
    // Potentially unfinished text.
    // Any number of digits at the beginning of the line, and possibly 1 "
    std::regex unfinishedGroup1(R"(^\d*"?$)");

    m_normalMap.m_countGroups.push_back(countGroup);
    m_normalMap.m_registerGroups.push_back(registerGroup);
    m_normalMap.m_unfinishedGroups.push_back(unfinishedGroup1);
    m_normalMap.ignoreFinalDigit = true;

    m_visualMap.m_countGroups.push_back(countGroup);
    m_visualMap.m_registerGroups.push_back(registerGroup);
    m_visualMap.m_unfinishedGroups.push_back(unfinishedGroup1);
    m_visualMap.ignoreFinalDigit = true;
    */
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
    m_dotCommand.clear();
    m_pendingEscape = false;
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

/*
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
    
   ... later if (key == 'j' && !m_pendingEscape)
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
    */
