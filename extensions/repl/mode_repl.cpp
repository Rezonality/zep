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

    // TODO: Modifiable but not saveable buffer
    m_pReplBuffer = GetEditor().GetEmptyBuffer("Repl.lisp");// , FileFlags::ReadOnly);
    m_pReplBuffer->SetBufferType(BufferType::Repl);
    m_pReplBuffer->GetSyntax()->IgnoreLineHighlight();

    // Adding the window will make it active and begin the mode
    m_pReplWindow = GetEditor().GetActiveTabWindow()->AddWindow(m_pReplBuffer, nullptr, RegionLayoutType::VBox);
    Prompt();
}

void ZepReplExCommand::Prompt()
{
    m_pReplBuffer->Insert(m_pReplBuffer->EndLocation(), PromptString);
    MoveToEnd();
}

void ZepReplExCommand::MoveToEnd()
{
    m_pReplWindow->SetBufferCursor(m_pReplBuffer->GetLinePos(m_pReplBuffer->EndLocation(), LineLocation::LineCRBegin));
    //m_startLocation = m_pCurrentWindow->GetBufferCursor();
}

void ZepReplExCommand::Notify(std::shared_ptr<ZepMessage> message)
{

    if (message->messageId == Msg::Buffer)
    {
        auto spBufferMsg = std::static_pointer_cast<BufferMessage>(message);
        if (spBufferMsg->pBuffer == m_pReplBuffer)
        {
            LOG(DEBUG) << "Buffer Message";
        }
    }
}

// TODO: Same keystroke command to close repl?

/*
void ZepMode_Repl::AddKeyPress(uint32_t key, uint32_t modifiers)
{
    // Set the cursor to the end of the buffer while inserting text
    GetCurrentWindow()->SetBufferCursor(MaxCursorMove);

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

    return;
}

*/

} // namespace Zep
