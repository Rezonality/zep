#include "buffer.h"
#include "display.h"
#include "tab_window.h"
#include "window.h"

// A 'window' is like a Vim Tab
namespace Zep
{

ZepTabWindow::ZepTabWindow(ZepDisplay& display)
    : ZepComponent(display.GetEditor())
    , m_display(display)
{
}

ZepTabWindow::~ZepTabWindow()
{
}

void ZepTabWindow::SetCurrentBuffer(ZepBuffer* pBuffer)
{
    AddBuffer(pBuffer);
    m_pCurrentBuffer = pBuffer;
}

ZepBuffer* ZepTabWindow::GetCurrentBuffer() const
{
    return m_pCurrentBuffer;
}

ZepWindow* ZepTabWindow::GetCurrentWindow() const
{
    auto itrFound = m_buffers.find(m_pCurrentBuffer);
    if (itrFound == m_buffers.end())
    {
        return nullptr;
    }
    return itrFound->second.get();
}

void ZepTabWindow::AddBuffer(ZepBuffer* pBuffer)
{
    auto itrFound = m_buffers.find(pBuffer);
    if (itrFound == m_buffers.end())
    {
        m_buffers[pBuffer] = std::make_shared<ZepWindow>(*this, *pBuffer, m_display);
    }
    if (m_pCurrentBuffer == nullptr)
    {
        m_pCurrentBuffer = pBuffer;
    }
}

void ZepTabWindow::RemoveBuffer(ZepBuffer* pBuffer)
{
    m_buffers.erase(pBuffer);
    if (m_pCurrentBuffer == pBuffer)
    {
        if (!m_buffers.empty())
        {
            SetCurrentBuffer(m_buffers.begin()->first);
        }
        else
        {
            SetCurrentBuffer(nullptr);
        }
    }
}

const ZepTabWindow::tBuffers& ZepTabWindow::GetBuffers() const
{
    return m_buffers;
}

void ZepTabWindow::Notify(std::shared_ptr<ZepMessage> payload)
{
    // Nothing yet.
}

void ZepTabWindow::PreDisplay(const DisplayRegion& region)
{
    m_windowRegion = region;

    m_buffersRegion = m_windowRegion;

    /*
    for (auto& buffer : m_buffers)
    {
        buffer.second->PreDisplay(m_buffersRegion);
    }*/

    // Just one buffer inside the window for now!
    if (m_pCurrentBuffer)
    {
        m_buffers[m_pCurrentBuffer]->PreDisplay(m_buffersRegion);
    }
}

void ZepTabWindow::Display()
{
    PreDisplay(m_windowRegion);
    
    if (m_pCurrentBuffer)
    {
        m_buffers[m_pCurrentBuffer]->Display();
    }
}

} // namespace Zep
