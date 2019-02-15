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

// Note:
// I added this window region management in one quick coding session, and it worked first time.
// At that point I should have documented what I did, because when a bug showed up much later, I didn't 
// understand clearly how it worked.  Having just fixed the bug, I realized a) I should have documented it better, and
// b) I have probably added too much code to fix the problem, when my earlier self would have known the correct quick fix!
// So this is a work item to document and clean up the split management.  But given that it now definately works correctly.....
// Anyway, I've added some more docs to help if this comes up again.
// The region management is in theory dirt-simple.
// Each region contains a list of either vertical or horizontally split regions.
// Any region can have child regions of the same type.
// If you split a window, you effectively create a stacked region within the parent region.
ZepWindow* ZepTabWindow::AddWindow(ZepBuffer* pBuffer, ZepWindow* pParent, bool vsplit)
{
    // Make a new window
    auto pWin = new ZepWindow(*this, pBuffer);
    m_windows.push_back(pWin);

    // This new window is going to introduce a new region
    auto r = std::make_shared<Region>();
    r->ratio = 1.0f;
    r->flags = RegionFlags::Expanding;

    m_pActiveWindow = pWin;

    // No parent, inserting our window on top of whatever is already in the window
    if (pParent == nullptr)
    {
        // There is something in the child, so make a good
        if (m_spRootRegion->children.size() > 1)
        {
            // Make a new parent to hold the children at the root, and replace the root region 
            // with the new parent, putting the old root inside it!
            auto r1 = std::make_shared<Region>();
            r1->ratio = 1.0f;
            r1->flags = RegionFlags::Expanding;
            r1->vertical = vsplit;
            r1->children.push_back(m_spRootRegion);
            m_spRootRegion->pParent = r1;
            m_spRootRegion = r1;
            r1->children.push_back(r);
        }
        else
        {
            // The root region has 0 or 1 children, so just set its split type and add our child
            m_spRootRegion->children.push_back(r);
            m_spRootRegion->vertical = vsplit;
        }

        // New region has root as the parent.
        r->pParent = m_spRootRegion;
        m_windowRegions[pWin] = r;
    }
    else
    {
        // Get the parent region that holds the parent window!
        std::shared_ptr<Region> pParentRegion = m_windowRegions[pParent]->pParent;
        if (pParentRegion->vertical == vsplit)
        {
            // Add our newly created region into the parent region, since it is splitting in the same way as the parent's children
            // Try to find the right spot, so we effectively split the correct parent
            auto itrFound = std::find_if(pParentRegion->children.begin(), pParentRegion->children.end(), [&](auto pCurrent)
            {
                if (pCurrent == m_windowRegions[pParent])
                {
                    return true;
                }
                return false;
            });

            // Insertion point should be _after_ the location we want
            if (itrFound != pParentRegion->children.end())
            {
                itrFound++;
            }
            pParentRegion->children.insert(itrFound, r);

            r->pParent = pParentRegion;
            m_windowRegions[pWin] = r;
        }
        else
        {
            // We are adding ourselves to a child that isn't the same orientation as us
            auto pSplitRegion = m_windowRegions[pParent];
            assert(pSplitRegion->children.empty());

            // Force the region to be of the new split type
            pSplitRegion->vertical = vsplit;

            // Make a new region and put the existing region in it
            auto r1 = std::make_shared<Region>();
            r1->ratio = 1.0f;
            r1->flags = RegionFlags::Expanding;
            pSplitRegion->children.push_back(r1);
            r1->pParent = pSplitRegion;
            m_windowRegions[pParent] = r1;

            // Put our new window on the end
            pSplitRegion->children.push_back(r);
            r->pParent = pSplitRegion;
            m_windowRegions[pWin] = r;
        }
    }

    m_pActiveWindow = pWin;

    SetDisplayRegion(m_lastRegionRect, true);

    LOG(INFO) << "AddWindow, Regions: ";
    LOG(INFO) << *m_spRootRegion;
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

// See AddWindow for comments..
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

    // Remove a child region from a parent, and clean up all children.
    // Also remove empty regions, cleaning up as we go
    std::function<void(std::shared_ptr<Region>, std::shared_ptr<Region>)> fnRemoveRegion = [&](auto parent, auto child)
    {
        // Remove all children
        while (child && !child->children.empty())
        {
            fnRemoveRegion(child, child->children[0]);
        }

        // Find our child in the parent
        if (parent)
        {
            auto itrFound = std::find_if(parent->children.begin(), parent->children.end(), [&child](auto pCurrent)
            {
                return pCurrent == child;
            });

            // Remove our region from the parent
            assert(itrFound != parent->children.end());
            if (itrFound != parent->children.end())
            {
                parent->children.erase(itrFound);
            }
        }

        child->pParent = nullptr;

        // If we are the last region, remove the last child 
        if (parent && parent->children.size() == 1)
        {
            if (parent != m_spRootRegion)
            {
                // This code runs if you vsplit, hsplit, :clo
                // What's happening here is that we have a 'parent' region with a child region that has only 1 child in it.
                // It's a redundant child.
                // So we find our redundant child in the global list of window regions, 
                // Then we re-register our window as being owned by parent, and destroy the child.
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
        m_spRootRegion.reset();
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
        assert(!m_spRootRegion->children.empty());
    
        LOG(INFO) << "RemoveWindow, Regions: ";
        LOG(INFO) << *m_spRootRegion;
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
