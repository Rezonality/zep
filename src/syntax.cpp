#include "syntax.h"
#include "editor.h"
#include "syntax_rainbow_brackets.h"
#include "theme.h"
#include "mcommon/string/stringutils.h"

#include <string>
#include <vector>

namespace Zep
{


ZepSyntax::ZepSyntax(
    ZepBuffer& buffer,
    const std::set<std::string>& keywords,
    const std::set<std::string>& identifiers,
    uint32_t flags)
    : ZepComponent(buffer.GetEditor())
    , m_buffer(buffer)
    , m_keywords(keywords)
    , m_identifiers(identifiers)
    , m_stop(false)
    , m_flags(flags)
{
    m_syntax.resize(m_buffer.GetText().size());
    m_adornments.push_back(std::make_shared<ZepSyntaxAdorn_RainbowBrackets>(*this, m_buffer));
    QueueUpdateSyntax(0, BufferLocation(m_buffer.GetText().size()));
}

ZepSyntax::~ZepSyntax()
{
    Interrupt();
}

NVec4f ZepSyntax::GetSyntaxColorAt(long offset) const
{
    Wait();
    if (m_processedChar < offset || ((long)m_syntax.size()) <= offset)
    {
        return m_buffer.GetTheme().GetColor(ThemeColor::Normal);
    }

    for (auto& adorn : m_adornments)
    {
        bool found = false;
        auto col = adorn->GetSyntaxColorAt(offset, found);
        if (found)
        {
            return col;
        }
    }

    return m_buffer.GetTheme().GetColor(m_syntax[offset]);
}

ThemeColor ZepSyntax::GetSyntaxAt(long offset) const
{
    Wait();
    if (m_processedChar < offset || (long)m_syntax.size() <= offset)
    {
        return ThemeColor::Normal;
    }

    return m_syntax[offset];
}

void ZepSyntax::Wait() const
{
    if (m_syntaxResult.valid())
    {
        m_syntaxResult.wait();
    }
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
    m_syntax.resize(m_buffer.GetText().size(), ThemeColor::Normal);

    m_processedChar = std::min(long(m_processedChar), long(m_buffer.GetText().size() - 1));
    m_targetChar = std::min(long(m_targetChar), long(m_buffer.GetText().size() - 1));

    // Have the thread update the syntax in the new region
    // If the pool has no threads, this will end up serial
    m_syntaxResult = GetEditor().GetThreadPool().enqueue([=]() {
            UpdateSyntax();
    });
}

void ZepSyntax::Notify(std::shared_ptr<ZepMessage> spMsg)
{
    // Handle any interesting buffer messages
    if (spMsg->messageId == Msg::Buffer)
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
            m_syntax.insert(m_syntax.begin() + spBufferMsg->startLocation, spBufferMsg->endLocation - spBufferMsg->startLocation, ThemeColor::Normal);
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
    assert(m_syntax.size() == buffer.size());

    std::string delim(" \t.\n;(){}=:");
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
    while (itrCurrent > buffer.begin() && *itrCurrent != '\n')
    {
        itrCurrent--;
    }
    itrEnd = buffer.find_first_of(itrEnd, buffer.end(), lineEnd.begin(), lineEnd.end());

    // Mark a region of the syntax buffer with the correct marker
    auto mark = [&](GapBuffer<utf8>::const_iterator itrA, GapBuffer<utf8>::const_iterator itrB, ThemeColor type) {
        std::fill(m_syntax.begin() + (itrA - buffer.begin()), m_syntax.begin() + (itrB - buffer.begin()), type);
    };

    auto markSingle = [&](GapBuffer<utf8>::const_iterator itrA, ThemeColor type) {
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
                mark(itr, itr + 1, ThemeColor::Whitespace);
            }
        }

        // Do I need to make a string here?
        auto token = std::string(itrFirst, itrLast);
        if (m_flags & ZepSyntaxFlags::CaseInsensitive)
        {
            token = string_tolower(token);
        }

        if (m_keywords.find(token) != m_keywords.end())
        {
            mark(itrFirst, itrLast, ThemeColor::Keyword);
        }
        else if (m_identifiers.find(token) != m_identifiers.end())
        {
            mark(itrFirst, itrLast, ThemeColor::Identifier);
        }
        else if (token.find_first_not_of("0123456789") == std::string::npos)
        {
            mark(itrFirst, itrLast, ThemeColor::Number);
        }
        else if (token.find_first_not_of("{}()[]") == std::string::npos)
        {
            mark(itrFirst, itrLast, ThemeColor::Parenthesis);
        }
        else
        {
            mark(itrFirst, itrLast, ThemeColor::Normal);
        }

        std::string stringStr = "\"";
        auto itrString = buffer.find_first_of(itrFirst, itrLast, stringStr.begin(), stringStr.end());
        if (itrString != buffer.end())
        {
            auto itrStringStart = itrString++;
            if (itrString < buffer.end())
            {
                itrLast = buffer.find_first_of(itrString, buffer.end(), stringStr.begin(), stringStr.end());
                if (itrLast < buffer.end())
                {
                    mark(itrStringStart, ++itrLast, ThemeColor::String);
                }
            }
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
                    mark(itrCommentStart, itrLast, ThemeColor::Comment);
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

} // namespace Zep
