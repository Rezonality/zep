#include "buffer.h"
#include "editor.h"
#include "display.h"
#include "tab_window.h"
#include "window.h"

// A 'window' is like a Vim Tab
namespace Zep
{

ZepTabWindow::ZepTabWindow(ZepEditor& editor)
    : ZepComponent(editor)
    , m_editor(editor)
{
}

ZepTabWindow::~ZepTabWindow()
{
    std::for_each(m_windows.begin(), m_windows.end(), [](ZepWindow* pWindow) { delete pWindow; });
}

ZepWindow* ZepTabWindow::AddWindow(ZepBuffer* pBuffer)
{
    auto pWin = new ZepWindow(*this, pBuffer);
    m_windows.push_back(pWin);

    if (m_pActiveWindow == nullptr)
    {
        m_pActiveWindow = pWin;
    }
    return pWin;
}

void ZepTabWindow::RemoveWindow(ZepWindow* pWindow)
{
    assert(pWindow);
    if (!pWindow)
        return;

    auto itrFound = std::find(m_windows.begin(), m_windows.end(), pWindow);
    if (itrFound == m_windows.end())
    {
        assert(!"Not found?");
        return;
    }

    delete pWindow;
    m_windows.erase(itrFound);
    if (m_pActiveWindow == pWindow)
    {
        m_pActiveWindow = nullptr;
    }
}

void ZepTabWindow::Notify(std::shared_ptr<ZepMessage> payload)
{
    // Nothing yet.
}

void ZepTabWindow::PreDisplay(ZepDisplay& display, const DisplayRegion& region)
{
    m_windowRegion = region;
    m_buffersRegion = m_windowRegion;

    for(auto& pWin : m_windows)
    {
        pWin->PreDisplay(display, m_buffersRegion);
    }
}

void ZepTabWindow::Display(ZepDisplay& display)
{
    PreDisplay(display, m_windowRegion);
    
    for(auto& pWin : m_windows)
    {
        pWin->Display(display);
    }
}

} // namespace Zep
