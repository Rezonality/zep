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
            totalFixedSize += region.vertical ? r->fixed_size.x : r->fixed_size.y;
        }
        else
        {
            expanders += 1.0f;
        }
    }

    NRectf currentRect = region.rect;
    auto remaining = (region.vertical ? currentRect.Width() : currentRect.Height()) - totalFixedSize;
    auto perExpanding = remaining / expanders;
    
    for (auto& r : region.children)
    {
        r->rect = currentRect;

        if (!region.vertical)
        {
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
        else
        {
            if (r->flags & RegionFlags::Fixed)
            {
                r->rect.bottomRightPx.x = currentRect.topLeftPx.x + r->fixed_size.x;
                currentRect.topLeftPx.x += r->fixed_size.x;
            }
            else
            {
                r->rect.bottomRightPx.x = currentRect.topLeftPx.x + perExpanding;
                currentRect.topLeftPx.x += perExpanding;
            }
        }
        r->rect.topLeftPx += r->margin;
        r->rect.bottomRightPx -= r->margin;
    }
    
    for (auto& r : region.children)
    {
        LayoutRegion(*r);
    }
}

}