#include "splits.h"

namespace Zep
{

void LayoutRegion(Region& region)
{
    float totalFixedSize = 0.0f;
    float expanders = 0.0f;
    for (auto& r : region.children)
    {
        if (r->flags & RegionFlags::Fixed)
        {
            totalFixedSize += r->fixed_size.y;
        }
        else
        {
            expanders += 1.0f;
        }
    }

    NRectf currentRect = region.rect;
    auto remaining = currentRect.Height() - totalFixedSize;
    auto perExpanding = remaining / expanders;
    
    for (auto& r : region.children)
    {
        r->rect = currentRect;
        if (r->flags & RegionFlags::Fixed)
        {
            r->rect.bottomRightPx.y = currentRect.topLeftPx.y + r->fixed_size.y;
            currentRect.topLeftPx.y += r->fixed_size.y;
        }
        else
        {
            r->rect.bottomRightPx.y = currentRect.topLeftPx.y + perExpanding;
            currentRect.topLeftPx.y += perExpanding;
        }
    }
    
    for (auto& r : region.children)
    {
        LayoutRegion(*r);
    }
}

}