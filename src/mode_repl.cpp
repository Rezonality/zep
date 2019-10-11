#include "zep/mode_repl.h"
#include "zep/filesystem.h"
#include "zep/tab_window.h"
#include "zep/window.h"
#include "zep/editor.h"

#include "zep/mcommon/logger.h"
#include "zep/mcommon/threadutils.h"

namespace Zep
{

const std::string PromptString = ">> ";
const std::string ContinuationString = ".. ";

ZepMode_Repl::ZepMode_Repl(ZepEditor& editor, ZepWindow& launchWindow, ZepWindow& replWindow)
    : ZepMode(editor),
    m_launchWindow(launchWindow),
    m_replWindow(replWindow)
{
    m_pRepl = m_launchWindow.GetBuffer().GetReplProvider();
}

ZepMode_Repl::~ZepMode_Repl()
{
}

void ZepMode_Repl::Close()
{
    GetEditor().RemoveBuffer(&m_replWindow.GetBuffer());
}

void ZepMode_Repl::AddKeyPress(uint32_t key, uint32_t modifiers)
{
    auto pGlobalMode = GetEditor().GetGlobalMode();

    GetEditor().ResetLastEditTimer();

    if (key == 'r' && modifiers == ModifierKey::Ctrl)
    {
        Close();
        return;
    }

    // If not in insert mode, then let the normal mode do its thing
    if (pGlobalMode->GetEditorMode() != Zep::EditorMode::Insert)
    {
        pGlobalMode->AddKeyPress(key, modifiers);

        m_currentMode = pGlobalMode->GetEditorMode();
        if (pGlobalMode->GetEditorMode() == Zep::EditorMode::Insert)
        {
            // Set the cursor to the end of the buffer while inserting text
            m_replWindow.SetBufferCursor(MaxCursorMove);
            m_replWindow.SetCursorType(CursorType::Insert);
        }
        return;
    }
  
    // Set the cursor to the end of the buffer while inserting text
    m_replWindow.SetBufferCursor(MaxCursorMove);
    m_replWindow.SetCursorType(CursorType::Insert);

    (void)modifiers;
    if (key == ExtKeys::ESCAPE)
    {
        // Escape back to the default normal mode
        GetEditor().GetGlobalMode()->Begin();
        GetEditor().GetGlobalMode()->SetEditorMode(EditorMode::Normal);
        m_currentMode = Zep::EditorMode::Normal;
        return;
    } 
    else if (key == ExtKeys::RETURN)
    {
        auto& buffer = m_replWindow.GetBuffer();
        std::string str = std::string(buffer.GetText().begin() + m_startLocation, buffer.GetText().end());
        buffer.Insert(buffer.EndLocation(), "\n");

        auto stripLineStarts = [](std::string& str)
        {
            bool newline = true;
            int pos = 0;
            while (pos < str.size())
            {
                if (str[pos] == '\n')
                    newline = true;
                else if (newline)
                {
                    if (str.find(PromptString, pos) == pos)
                    {
                        str.erase(pos, PromptString.length());
                        continue;
                    }
                    else if (str.find(ContinuationString, pos) == pos)
                    {
                        str.erase(pos, ContinuationString.length());
                        continue;
                    }
                    newline = false;
                }
                pos++;
            }
        };
        stripLineStarts(str);

        std::string ret;
        if (m_pRepl)
        {
            int indent = 0;
            bool complete = m_pRepl->fnIsFormComplete ? m_pRepl->fnIsFormComplete(str, indent) : true;
            if (!complete)
            {
                // If the indent is < 0, we completed too much of the expression, so don't let the user hit return until they 
                // fix it.  Example in lisp: (+ 2 2))  This expression has 'too many' close brackets.
                if (indent < 0)
                {
                    buffer.Delete(buffer.EndLocation() - 1, buffer.EndLocation());
                    m_replWindow.SetBufferCursor(MaxCursorMove);
                    return;
                }
                   
                // New line continuation symbol
                buffer.Insert(buffer.EndLocation(), ContinuationString);

                // Indent by how far the repl suggests
                if (indent > 0)
                {
                    for (int i = 0; i < indent; i++)
                    {
                        buffer.Insert(buffer.EndLocation(), " ");
                    }
                }
                m_replWindow.SetBufferCursor(MaxCursorMove);
                return;
            }
            ret = m_pRepl->fnParser(str);
        }
        else
        {
            ret = str;
        }

        if (!ret.empty() && ret[0] != 0)
        {
            ret.push_back('\n');
            buffer.Insert(buffer.EndLocation(), ret);
        }

        BeginInput();
        return;
    }
    else if (key == ExtKeys::BACKSPACE)
    {
        auto cursor = m_replWindow.GetBufferCursor() - 1;
        if (cursor >= m_startLocation)
        {
            m_replWindow.GetBuffer().Delete(m_replWindow.GetBufferCursor() - 1, m_replWindow.GetBufferCursor());
        }
    }
    else
    {
        char c[2];
        c[0] = (char)key;
        c[1] = 0;
        m_replWindow.GetBuffer().Insert(m_replWindow.GetBufferCursor(), std::string(c));
    }

    // Ensure cursor is at buffer end
    m_replWindow.SetBufferCursor(MaxCursorMove);
}

void ZepMode_Repl::BeginInput()
{
    // Input arrows
    auto& buffer = m_replWindow.GetBuffer();
    buffer.Insert(buffer.EndLocation(), PromptString);

    m_replWindow.SetBufferCursor(MaxCursorMove);
    m_startLocation = m_replWindow.GetBufferCursor();
}

void ZepMode_Repl::Begin()
{
    // Default insert mode
    GetEditor().GetGlobalMode()->Begin();
    GetEditor().GetGlobalMode()->SetEditorMode(EditorMode::Insert);
    m_currentMode = EditorMode::Insert;
    m_replWindow.SetCursorType(CursorType::Insert);

    GetEditor().SetCommandText("");
    
    BeginInput();
}

void ZepMode_Repl::Notify(std::shared_ptr<ZepMessage> message)
{
    ZepMode::Notify(message);
}

} // namespace Zep
