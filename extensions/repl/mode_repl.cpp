#include "zep/editor.h"
#include "zep/filesystem.h"
#include "zep/syntax.h"
#include "zep/tab_window.h"
#include "zep/window.h"

#include "zep/mcommon/logger.h"
#include "zep/mcommon/threadutils.h"

#include "mode_repl.h"
namespace Zep
{

const std::string PromptString = ">> ";
const std::string ContinuationString = ".. ";

ZepReplExCommand::ZepReplExCommand(ZepEditor& editor, IZepReplProvider* pProvider)
    : ZepExCommand(editor)
    , m_pProvider(pProvider)
{
    keymap_add(m_keymap, { "<c-e>" }, ExCommandId());
}

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

    auto pReplBuffer = GetEditor().GetEmptyBuffer("Repl.lisp", FileFlags::Locked);
    pReplBuffer->SetBufferType(BufferType::Repl);
    pReplBuffer->GetSyntax()->IgnoreLineHighlight();

    // Note: Need to set the mode _before_ adding the window
    auto pMode = std::make_shared<ZepMode_Repl>(GetEditor(), m_pProvider);
    pReplBuffer->SetMode(pMode);

    // Adding the window will make it active and begin the mode
    GetEditor().GetActiveTabWindow()->AddWindow(pReplBuffer, nullptr, RegionLayoutType::VBox);
    pMode->Prompt();
}

ZepReplEvaluateCommand::ZepReplEvaluateCommand(ZepEditor& editor, IZepReplProvider* pProvider)
    : ZepExCommand(editor)
    , m_pProvider(pProvider)
{
    keymap_add(m_keymap, { "<C-Return>" }, ExCommandId());
}

void ZepReplEvaluateCommand::Register(ZepEditor& editor, IZepReplProvider* pProvider)
{
    editor.RegisterExCommand(std::make_shared<ZepReplEvaluateCommand>(editor, pProvider));
}

void ZepReplEvaluateCommand::Run(const std::vector<std::string>& tokens)
{
    ZEP_UNUSED(tokens);
    if (!GetEditor().GetActiveTabWindow())
    {
        return;
    }

    auto& buffer = GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBuffer();
    auto cursor = GetEditor().GetActiveTabWindow()->GetActiveWindow()->GetBufferCursor();

    m_pProvider->ReplParse(buffer, cursor, ReplParseType::OuterExpression);
}

ZepMode_Repl::ZepMode_Repl(ZepEditor& editor, IZepReplProvider* pProvider)
    : ZepMode(editor)
    , m_pRepl(pProvider)
{
}

ZepMode_Repl::~ZepMode_Repl()
{
}

void ZepMode_Repl::Close()
{
    if (m_pCurrentWindow == nullptr)
    {
        return;
    }
    GetEditor().RemoveBuffer(&GetCurrentWindow()->GetBuffer());
}

void ZepMode_Repl::AddKeyPress(uint32_t key, uint32_t modifiers)
{
    if (m_pCurrentWindow == nullptr)
    {
        return;
    }
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
            GetCurrentWindow()->SetBufferCursor(MaxCursorMove);
        }
        return;
    }

    // Set the cursor to the end of the buffer while inserting text
    GetCurrentWindow()->SetBufferCursor(MaxCursorMove);

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
        auto& buffer = GetCurrentWindow()->GetBuffer();
        std::string str = std::string(buffer.GetText().begin() + m_startLocation, buffer.GetText().end());
        buffer.Insert(buffer.EndLocation(), "\n");

        auto stripLineStarts = [](std::string& str) {
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
                    GetCurrentWindow()->SetBufferCursor(MaxCursorMove);
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
                GetCurrentWindow()->SetBufferCursor(MaxCursorMove);
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

        Prompt();
        return;
    }
    else if (key == ExtKeys::BACKSPACE)
    {
        auto cursor = GetCurrentWindow()->GetBufferCursor() - 1;
        if (cursor >= m_startLocation)
        {
            GetCurrentWindow()->GetBuffer().Delete(GetCurrentWindow()->GetBufferCursor() - 1, GetCurrentWindow()->GetBufferCursor());
        }
    }
    else
    {
        char c[2];
        c[0] = (char)key;
        c[1] = 0;
        GetCurrentWindow()->GetBuffer().Insert(GetCurrentWindow()->GetBufferCursor(), std::string(c));
    }

    // Ensure cursor is at buffer end
    GetCurrentWindow()->SetBufferCursor(MaxCursorMove);

    return;
}

void ZepMode_Repl::Prompt()
{
    if (m_pCurrentWindow == nullptr)
    {
        return;
    }

    // Input arrows
    auto& buffer = m_pCurrentWindow->GetBuffer();
    buffer.Insert(buffer.EndLocation(), PromptString);

    MoveToEnd();
}

void ZepMode_Repl::MoveToEnd()
{
    if (m_pCurrentWindow == nullptr)
    {
        return;
    }

    // Input arrows
    auto& buffer = m_pCurrentWindow->GetBuffer();
    m_pCurrentWindow->SetBufferCursor(buffer.GetLinePos(buffer.EndLocation(), LineLocation::LineCRBegin));
    m_startLocation = m_pCurrentWindow->GetBufferCursor();
}

void ZepMode_Repl::Begin(ZepWindow* pWindow)
{
    ZepMode::Begin(pWindow);

    // Default insert mode
    SwitchMode(EditorMode::Insert);

    GetEditor().SetCommandText("");

    MoveToEnd();
}

void ZepMode_Repl::Notify(std::shared_ptr<ZepMessage> message)
{
    ZepMode::Notify(message);
}

} // namespace Zep
