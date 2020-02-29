#include "zep/filesystem.h"
#include "zep/tab_window.h"
#include "zep/window.h"
#include "zep/editor.h"

#include "zep/mcommon/logger.h"
#include "zep/mcommon/threadutils.h"

#include "mode_repl.h"
namespace Zep
{

const std::string PromptString = ">> ";
const std::string ContinuationString = ".. ";

void ZepReplExCommand::Register(ZepEditor& editor, IZepReplProvider* pProvider)
{
    editor.RegisterExCommand(std::make_shared<ZepReplExCommand>(editor, pProvider));
}

void ZepReplExCommand::Run(const std::vector<std::string>& tokens)
{
    ZEP_UNUSED(tokens);
    if (!GetEditor().GetActiveTabWindow())
    {
        return;
    }

    auto pActiveWindow = GetEditor().GetActiveTabWindow()->GetActiveWindow();

    auto pReplBuffer = GetEditor().GetEmptyBuffer("Repl.lisp", FileFlags::Locked);
    pReplBuffer->SetBufferType(BufferType::Repl);

    auto pReplWindow = GetEditor().GetActiveTabWindow()->AddWindow(pReplBuffer, nullptr, RegionLayoutType::VBox);

    auto pMode = std::make_shared<ZepMode_Repl>(GetEditor(), *pActiveWindow, *pReplWindow, m_pProvider);
    pReplBuffer->SetMode(pMode);
    pMode->Init();
    pMode->Begin(pReplWindow);
    pMode->SwitchMode(Zep::EditorMode::Insert);
}

ZepMode_Repl::ZepMode_Repl(ZepEditor& editor, ZepWindow& launchWindow, ZepWindow& replWindow, IZepReplProvider* pProvider)
    : ZepMode(editor),
    m_launchWindow(launchWindow),
    m_replWindow(replWindow),
    m_pRepl(pProvider)
{
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
    auto pMode = m_pCurrentWindow->GetBuffer().GetMode();

    GetEditor().ResetLastEditTimer();

    if (key == 'r' && modifiers == ModifierKey::Ctrl)
    {
        Close();
        return;
    }

    // If not in insert mode, then let the normal mode do its thing
    if (pMode->GetEditorMode() != Zep::EditorMode::Insert)
    {
        // Defer to the global mode for everything that isn't insert
        GetEditor().GetGlobalMode()->AddKeyPress(key, modifiers);

        // Switched out of the global mode, so back into insert in this mode
        if (GetEditor().GetGlobalMode()->GetEditorMode() == Zep::EditorMode::Insert)
        {
            m_pCurrentWindow->SetBufferCursor(MaxCursorMove);
            pMode->SwitchMode(EditorMode::Insert);
        }

        m_currentMode = pMode->GetEditorMode();
        if (pMode->GetEditorMode() == Zep::EditorMode::Insert)
        {
            // Set the cursor to the end of the buffer while inserting text
            m_replWindow.SetBufferCursor(MaxCursorMove);
        }
        return;
    }
  
    // Set the cursor to the end of the buffer while inserting text
    m_replWindow.SetBufferCursor(MaxCursorMove);

    (void)modifiers;
    if (key == ExtKeys::ESCAPE)
    {
        // Escape back to the default normal mode
        SwitchMode(EditorMode::Normal);
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
            bool complete = m_pRepl->ReplIsFormComplete(str, indent);
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
            ret = m_pRepl->ReplParse(str);
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
    
    return;
}

void ZepMode_Repl::BeginInput()
{
    // Input arrows
    auto& buffer = m_replWindow.GetBuffer();
    buffer.Insert(buffer.EndLocation(), PromptString);

    m_replWindow.SetBufferCursor(buffer.GetLinePos(buffer.EndLocation(), LineLocation::LineLastGraphChar) + 1);

    m_startLocation = m_replWindow.GetBufferCursor();
}

void ZepMode_Repl::Begin(ZepWindow* pWindow)
{
    ZepMode::Begin(pWindow);

    // Default insert mode
    SwitchMode(EditorMode::Insert);

    GetEditor().SetCommandText("");
    
    BeginInput();
}

void ZepMode_Repl::Notify(std::shared_ptr<ZepMessage> message)
{
    ZepMode::Notify(message);
}

} // namespace Zep
