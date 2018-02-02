#include "syntax.h"
#include "editor.h"

namespace Zep
{

ZepSyntax::ZepSyntax(ZepBuffer& buffer)
    : ZepComponent(buffer.GetEditor()),
    m_buffer(buffer)
{
    m_syntax.resize(m_buffer.GetText().size());
}

ZepSyntax::~ZepSyntax()
{
}

void ZepSyntax::QueueUpdateSyntax(BufferLocation startLocation, BufferLocation endLocation)
{
    // Stop the syntax thread, since it is no longer valid
    Interrupt();

    // Record the max location the syntax is valid up to.  This will 
    // ensure that multiple calls to restart the thread keep track of where to start
    // This means a small edit at the end of a big file, followed by a small edit at the top
    // is the worst case scenario, because 
    m_processedChar = std::min(startLocation, long(m_processedChar));
    m_targetChar = std::max(endLocation, long(m_targetChar));

    if (m_syntax.size() > endLocation)
    {
        // This region is 'invalid'; update it
        std::fill(m_syntax.begin() + m_processedChar, m_syntax.begin() + m_targetChar, SyntaxType::Normal);
    }

    // Make sure the syntax buffer is big enough - adding normal syntax to the end
    // This may also 'chop'
    m_syntax.resize(m_buffer.GetText().size(), SyntaxType::Normal);

    m_processedChar = std::min(long(m_processedChar), long(m_buffer.GetText().size() - 1));
    m_targetChar = std::min(long(m_targetChar), long(m_buffer.GetText().size() - 1));

    m_processedChar = 0;
    m_targetChar = long(m_buffer.GetText().size() - 1);
    // "Update Syntax, Range: " << m_processedChar << "-" << m_targetChar;

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
            QueueUpdateSyntax(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
        else if (spBufferMsg->type == BufferMessageType::TextAdded)
        {
            Interrupt();
            QueueUpdateSyntax(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
        else if (spBufferMsg->type == BufferMessageType::TextChanged)
        {
            Interrupt();
            QueueUpdateSyntax(spBufferMsg->startLocation, spBufferMsg->endLocation);
        }
    }
}

} // Zep
