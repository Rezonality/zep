#include "tab_window.h"
#include "buffer.h"
#include "display.h"
#include "editor.h"
#include "window.h"

// A 'window' is like a Vim Tab
namespace Zep
{

ZepTabWindow::ZepTabWindow(ZepEditor& editor)
    : ZepComponent(editor)
    , m_editor(editor)
{
    m_spRootRegion = std::make_shared<Region>();
    m_spRootRegion->ratio = 1.0f;
    m_spRootRegion->flags = RegionFlags::Expanding;
}

ZepTabWindow::~ZepTabWindow()
{
    std::for_each(m_windows.begin(), m_windows.end(), [](ZepWindow* w) { delete w; });
}

ZepWindow* ZepTabWindow::AddWindow(ZepBuffer* pBuffer, ZepWindow* pParent, bool vsplit)
{
    // Make a new window
    auto pWin = new ZepWindow(*this, pBuffer);
    m_windows.push_back(pWin);

    // This new window is going to introduce a new region
    auto r = std::make_shared<Region>();
    r->ratio = 1.0f;
    r->flags = RegionFlags::Expanding;

    // No active windows yet, so make this one the active
    if (m_pActiveWindow == nullptr)
    {
        m_pActiveWindow = pWin;
    }

    Region* pParentRegion = nullptr;
    if (pParent == nullptr)
    {
        // If there is no parent window, then we are adding this window into the root region
        pParentRegion = m_spRootRegion.get();
        pParentRegion->vertical = vsplit;
    }
    else
    {
        // Get the parent region that holds the parent window!
        pParentRegion = m_windowRegions[pParent]->pParent;
    }

    if (pParentRegion->vertical == vsplit)
    {
        // Add our newly created region into the parent
        pParentRegion->children.push_back(r);
        r->pParent = pParentRegion;
        m_windowRegions[pWin] = r;
    }
    else
    {
        auto pSplitRegion = m_windowRegions[pParent];
        assert(pSplitRegion->children.empty());

        pSplitRegion->vertical = vsplit;

        auto r1 = std::make_shared<Region>();
        r1->ratio = 1.0f;
        r1->flags = RegionFlags::Expanding;
        pSplitRegion->children.push_back(r1);
        r1->pParent = pSplitRegion.get();
        m_windowRegions[pParent] = r1;

        pSplitRegion->children.push_back(r);
        r->pParent = pSplitRegion.get();
        m_windowRegions[pWin] = r;
    }

    SetDisplayRegion(m_lastRegionRect, true);
    return pWin;
}

void ZepTabWindow::CloseActiveWindow()
{
    if (m_pActiveWindow)
    {
        RemoveWindow(m_pActiveWindow);
    }
    SetDisplayRegion(m_lastRegionRect, true);
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
    m_windowRegions.erase(pWindow);

    if (m_windows.empty())
    {
        m_pActiveWindow = nullptr;
        GetEditor().RemoveTabWindow(this);
    }
    else
    {
        if (m_pActiveWindow == pWindow)
        {
            // TODO: Active window ordering - remember the last active and switch to it when this one is closed
            m_pActiveWindow = m_windows[m_windows.size() - 1];
        }
    }
    SetDisplayRegion(m_lastRegionRect, true);
}

void ZepTabWindow::Notify(std::shared_ptr<ZepMessage> payload)
{
    // Nothing yet.
}

void ZepTabWindow::SetDisplayRegion(const NRectf& region, bool force)
{
    if (m_lastRegionRect != region || force)
    {
        m_lastRegionRect = region;
        m_spRootRegion->rect = region;
        LayoutRegion(*m_spRootRegion);

        for (auto& w : m_windows)
        {
            w->SetDisplayRegion(m_windowRegions[w]->rect);
        }
    }
}

void ZepTabWindow::Display()
{
    for (auto& w : m_windows)
    {
        w->Display();
    }
}

} // namespace Zep
