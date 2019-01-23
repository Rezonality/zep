#include "tab_window.h"
#include "buffer.h"
#include "display.h"
#include "editor.h"
#include "window.h"
#include "mcommon/logger.h"

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
    m_spRootRegion->pszName = "Root";
}

ZepTabWindow::~ZepTabWindow()
{
    std::for_each(m_windows.begin(), m_windows.end(), [](ZepWindow* w) { delete w; });
}

ZepWindow* ZepTabWindow::DoMotion(WindowMotion motion)
{
    if (!m_pActiveWindow)
        return nullptr;

    auto pCurrentRegion = m_windowRegions[m_pActiveWindow];
    auto center = pCurrentRegion->rect.Center();
    auto minDistance = std::numeric_limits<float>::max();
    Region* pBestRegion = nullptr;

    auto overlap_x = [&](const NRectf& lhs, const NRectf& rhs)
    {
        if ((lhs.topLeftPx.x < rhs.bottomRightPx.x) && 
            (lhs.bottomRightPx.x > rhs.topLeftPx.x))
        {
            return true;
        }
        return false;
    };
    auto overlap_y = [&](const NRectf& lhs, const NRectf& rhs)
    {
        if ((lhs.topLeftPx.y < rhs.bottomRightPx.y) && 
            (lhs.bottomRightPx.y > rhs.topLeftPx.y))
        {
            return true;
        }
        return false;
    };

    for (auto& r : m_windowRegions)
    {
        auto& pRegion = r.second;
        if (pCurrentRegion == pRegion)
            continue;

        auto thisCenter = pRegion->rect.Center();
        float dist = 0.0f;
        switch (motion)
        {
            case WindowMotion::Right:
            {
                if (overlap_x(pRegion->rect, pCurrentRegion->rect))
                    continue;
                if (!overlap_y(pRegion->rect, pCurrentRegion->rect))
                    continue;
                dist = thisCenter.x - center.x;
            }
            break;
            case WindowMotion::Left:
            {
                if (overlap_x(pRegion->rect, pCurrentRegion->rect))
                    continue;
                if (!overlap_y(pRegion->rect, pCurrentRegion->rect))
                    continue;
                dist = center.x - thisCenter.x;
            }
            break;
            case WindowMotion::Up:
            {
                if (overlap_y(pRegion->rect, pCurrentRegion->rect))
                    continue;
                if (!overlap_x(pRegion->rect, pCurrentRegion->rect))
                    continue;
                dist = center.y - thisCenter.y;
            }
            break;
            case WindowMotion::Down:
            {
                if (overlap_y(pRegion->rect, pCurrentRegion->rect))
                    continue;
                if (!overlap_x(pRegion->rect, pCurrentRegion->rect))
                    continue;
                dist = thisCenter.y - center.y;
            }
            break;
        }
        if (dist > 0.0f &&
            minDistance > dist)
        {
            minDistance = dist;
            pBestRegion = pRegion.get();
        }
    }

    if (pBestRegion != nullptr)
    {
        auto itrFound = std::find_if(m_windowRegions.begin(), m_windowRegions.end(), [pBestRegion](auto& val) { return (val.second.get() == pBestRegion); });
        if (itrFound != m_windowRegions.end())
        {
            m_pActiveWindow = itrFound->first;
        }
    }
    return m_pActiveWindow;
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

    std::shared_ptr<Region> pParentRegion;
    if (pParent == nullptr)
    {
        // If there is no parent window, then we are adding this window into the root region
        pParentRegion = m_spRootRegion;
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
        r1->pParent = pSplitRegion;
        m_windowRegions[pParent] = r1;

        pSplitRegion->children.push_back(r);
        r->pParent = pSplitRegion;
        m_windowRegions[pWin] = r;
    }

    m_pActiveWindow = pWin;

    SetDisplayRegion(m_lastRegionRect, true);
    
    return pWin;
}

void ZepTabWindow::CloseActiveWindow()
{
    if (m_pActiveWindow)
    {
        RemoveWindow(m_pActiveWindow);
    }

    if (!m_windows.empty())
    {
        SetDisplayRegion(m_lastRegionRect, true);
    }
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

    auto pRegion = m_windowRegions[pWindow];
    /*
    LOG(DEBUG) << *m_spRootRegion;
    LOG(DEBUG) << "Removing: " << std::hex << pRegion.get();
    */

    std::function<void(std::shared_ptr<Region>, std::shared_ptr<Region>)> fnRemoveRegion = [&](auto parent, auto child)
    {
        while (!child->children.empty())
        {
            fnRemoveRegion(child, child->children[0]);
        }

        auto itrFound = std::find_if(parent->children.begin(), parent->children.end(), [&child](auto pCurrent)
        {
            return pCurrent == child;
        });

        if (itrFound == parent->children.end())
        {
            return;
        }

        child->pParent = nullptr;
        parent->children.erase(itrFound);

        // If we are the last region, remove the last child 
        if (parent->children.size() == 1)
        {
            if (parent != m_spRootRegion)
            {
                auto itrWin = std::find_if(m_windowRegions.begin(), m_windowRegions.end(), [&](auto pr)
                {
                    return (pr.second == parent->children[0]);
                });
                if (itrWin != m_windowRegions.end())
                {
                    auto pWin = itrWin->first;
                    m_windowRegions.erase(itrWin);
                    m_windowRegions[pWin] = parent;
                }
                parent->children.clear();
            }
        }
    };
    fnRemoveRegion(pRegion->pParent, pRegion);

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
        SetDisplayRegion(m_lastRegionRect, true);
    }
}

void ZepTabWindow::Notify(std::shared_ptr<ZepMessage> pMsg)
{
    if (pMsg->messageId == Msg::MouseDown)
    {
        for (auto& region : m_windowRegions)
        {
            if (region.second->rect.Contains(pMsg->pos))
            {
                if (m_pActiveWindow != region.first)
                {
                    SetActiveWindow(region.first);
                    pMsg->handled = true;
                    return;
                }
            }
        }
    }
}

void ZepTabWindow::SetDisplayRegion(const NRectf& region, bool force)
{
    if (m_lastRegionRect != region || force)
    {
        m_lastRegionRect = region;

        if (m_spRootRegion)
        {
            m_spRootRegion->rect = region;
            LayoutRegion(*m_spRootRegion);
        }

        for (auto& w : m_windows)
        {
            w->SetDisplayRegion(m_windowRegions[w]->rect);
        }
    }
    /*
    LOG(DEBUG) << *m_spRootRegion;
    LOG(DEBUG) << "Num Win Regions: " << m_windowRegions.size();
    */
}

void ZepTabWindow::Display()
{
    for (auto& w : m_windows)
    {
        w->Display();
    }
}

} // namespace Zep
