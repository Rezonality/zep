#include "syntax.h"
#include "editor.h"

namespace Zep
{

ZepSyntax::ZepSyntax(ZepBuffer& buffer)
    : ZepComponent(buffer.GetEditor()),
    m_buffer(buffer),
    m_stop(false)
{
    m_syntax.resize(m_buffer.GetText().size());
}

ZepSyntax::~ZepSyntax()
{
    Interrupt();
}

uint32_t ZepSyntax::GetSyntaxAt(long offset) const
{
    if (m_processedChar < offset ||
        m_syntax.size() <= offset)
    {
        return SyntaxType::Normal;
    }

    return m_syntax[offset];
}

void ZepSyntax::Interrupt()
{
    // Stop the thread, wait for it
    m_stop = true;
    if (m_syntaxResult.valid())
    {
        m_syntaxResult.get();
    }
    m_stop = false;
}

void ZepSyntax::QueueUpdateSyntax(BufferLocation startLocation, BufferLocation endLocation)
{
    assert(startLocation >= 0);
    assert(endLocation >= startLocation);
    // Record the max location the syntax is valid up to.  This will 
    // ensure that multiple calls to restart the thread keep track of where to start
    // This means a small edit at the end of a big file, followed by a small edit at the top
    // is the worst case scenario, because 
    m_processedChar = std::min(startLocation, long(m_processedChar));
    m_targetChar = std::max(endLocation, long(m_targetChar));

    // Make sure the syntax buffer is big enough - adding normal syntax to the end
    // This may also 'chop'
    m_syntax.resize(m_buffer.GetText().size(), SyntaxType::Normal);

    m_processedChar = std::min(long(m_processedChar), long(m_buffer.GetText().size() - 1));
    m_targetChar = std::min(long(m_targetChar), long(m_buffer.GetText().size() - 1));

    // Have the thread update the syntax in the new region
    if (GetEditor().GetFlags() & ZepEditorFlags::DisableThreads)
    {
        UpdateSyntax();
    }
    else
    {
        m_syntaxResult = m_buffer.GetThreadPool().enqueue([=]()
        {
            UpdateSyntax(); 
        });
    }
}

void ZepSyntax::Notify(std::shared_ptr<ZepMessage> spMsg)
{
    // Handle any interesting buffer messages
    if (spMsg->messageId == Msg_Buffer)
    {
        auto spBufferMsg = std::static_pointer_cast<BufferMessage>(spMsg);
        if (spBufferMsg->pBuffer != &m_buffer)
        {
            return;
        }
        if (spBufferMsg->type == BufferMessageType::PreBufferChange)
        {
            Interrupt();
        }
        else if (spBufferMsg->type == BufferMessageType::TextDeleted)
        {
            Interrupt();
            m_syntax.erase(m_syntax.begin() + spBufferMsg->startLocation, m_syntax.begin() + spBufferMsg->endLocation);
            QueueUpdateSyntax(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
        else if (spBufferMsg->type == BufferMessageType::TextAdded)
        {
            Interrupt();
            m_syntax.insert(m_syntax.begin() + spBufferMsg->startLocation,
                spBufferMsg->endLocation - spBufferMsg->startLocation,
                SyntaxType::Normal);
            QueueUpdateSyntax(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
        else if (spBufferMsg->type == BufferMessageType::TextChanged)
        {
            Interrupt();
            QueueUpdateSyntax(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
    }
}

// TODO: Multiline comments
void ZepSyntax::UpdateSyntax()
{
    auto& buffer = m_buffer.GetText();
    auto itrCurrent = buffer.begin() + m_processedChar;
    auto itrEnd = buffer.begin() + m_targetChar;

    assert(std::distance(itrCurrent, itrEnd) < int(m_syntax.size()));

    std::string delim(" \t.\n;(){}=");
    std::string lineEnd("\n");

    // Walk backwards to previous delimiter
    while (itrCurrent > buffer.begin())
    {
        if (std::find(delim.begin(), delim.end(), *itrCurrent) == delim.end())
        {
            itrCurrent--;
        }
        else
        {
            break;
        }
    }

    // Back to the previous line
    while (itrCurrent > buffer.begin() &&
        *itrCurrent != '\n')
    {
        itrCurrent--;
    }
    itrEnd = buffer.find_first_of(itrEnd, buffer.end(), lineEnd.begin(), lineEnd.end());

    // Mark a region of the syntax buffer with the correct marker
    auto mark = [&](GapBuffer<utf8>::const_iterator itrA, GapBuffer<utf8>::const_iterator itrB, uint32_t type)
    {
        std::fill(m_syntax.begin() + (itrA - buffer.begin()), m_syntax.begin() + (itrB - buffer.begin()), type);
    };

    auto markSingle = [&](GapBuffer<utf8>::const_iterator itrA, uint32_t type)
    {
        *(m_syntax.begin() + (itrA - buffer.begin())) = type;
    };

    // Update start location
    m_processedChar = long(itrCurrent - buffer.begin());

    // Walk the buffer updating information about syntax coloring
    while (itrCurrent != itrEnd)
    {
        if (m_stop == true)
        {
            return;
        }

        // Find a token, skipping delim <itrFirst, itrLast>
        auto itrFirst = buffer.find_first_not_of(itrCurrent, buffer.end(), delim.begin(), delim.end());
        if (itrFirst == buffer.end())
            break;

        auto itrLast = buffer.find_first_of(itrFirst, buffer.end(), delim.begin(), delim.end());

        // Ensure we found a token
        assert(itrLast >= itrFirst);

        // Mark whitespace
        for (auto& itr = itrCurrent; itr < itrFirst; itr++)
        {
            if (*itr == ' ')
            {
                mark(itr, itr + 1, SyntaxType::Whitespace);
            }
        }

        // Do I need to make a string here?
        auto token = std::string(itrFirst, itrLast);
        if (keywords.find(token) != keywords.end())
        {
            mark(itrFirst, itrLast, SyntaxType::Keyword);
        }
        else if (token.find_first_not_of("0123456789") == std::string::npos)
        {
            mark(itrFirst, itrLast, SyntaxType::Integer);
        }
        else
        {
            mark(itrFirst, itrLast, SyntaxType::Normal);
        }

        std::string commentStr = "/";
        auto itrComment = buffer.find_first_of(itrFirst, itrLast, commentStr.begin(), commentStr.end());
        if (itrComment != buffer.end())
        {
            auto itrCommentStart = itrComment++;
            if (itrComment < buffer.end())
            {
                if (*itrComment == '/')
                {
                    itrLast = buffer.find_first_of(itrCommentStart, buffer.end(), lineEnd.begin(), lineEnd.end());
                    mark(itrCommentStart, itrLast, SyntaxType::Comment);
                }
            }
        }
        itrCurrent = itrLast;
    }

    // If we got here, we sucessfully completed
    // Reset the target to the beginning
    m_targetChar = long(0);
    m_processedChar = long(buffer.size() - 1);
}

} // Zep
