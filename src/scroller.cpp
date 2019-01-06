#include "common_namespace.h"
#include "editor.h"
#include "theme.h"
#include "scroller.h"
#include "display.h"
#include "logger.h"

namespace Zep
{


void Scroller_Init(Scroller& scroller, Region& parent)
{
    scroller.region = std::make_shared<Region>();
    scroller.topButtonRegion = std::make_shared<Region>();
    scroller.bottomButtonRegion = std::make_shared<Region>();
    scroller.mainRegion = std::make_shared<Region>();

    scroller.region->flags = RegionFlags::Expanding;
    scroller.topButtonRegion->flags = RegionFlags::Fixed;
    scroller.bottomButtonRegion->flags = RegionFlags::Fixed;
    scroller.mainRegion->flags = RegionFlags::Expanding;

    scroller.region->vertical = false;

    scroller.topButtonRegion->margin = NVec2f(4.0f, 3.0f);
    scroller.bottomButtonRegion->margin = NVec2f(4.0f, 3.0f);
    scroller.mainRegion->margin = NVec2f(4.0f, 0.0f);

    const float scrollButtonBorder = 2.0f;
    const float scrollButtonSize = 16.0f;
    scroller.topButtonRegion->fixed_size = NVec2f(0.0f, scrollButtonSize);
    scroller.bottomButtonRegion->fixed_size = NVec2f(0.0f, scrollButtonSize);

    scroller.region->children.push_back(scroller.topButtonRegion);
    scroller.region->children.push_back(scroller.mainRegion);
    scroller.region->children.push_back(scroller.bottomButtonRegion);

    parent.children.push_back(scroller.region);
}

void Scroller_Display(Scroller& scroller, IZepDisplay& display, ZepTheme& theme)
{
    display.SetClipRect(scroller.region->rect);

    float thumbSize = scroller.mainRegion->rect.Height() * scroller.vScrollVisiblePercent;

    // Scroller background
    display.DrawRectFilled(scroller.region->rect, theme.GetColor(ThemeColor::WidgetBackground));

    display.DrawRectFilled(scroller.topButtonRegion->rect, theme.GetColor(ThemeColor::WidgetActive));
    display.DrawRectFilled(scroller.bottomButtonRegion->rect, theme.GetColor(ThemeColor::WidgetActive));

    // Thumb
    display.DrawRectFilled(
        NRectf(NVec2f(scroller.mainRegion->rect.topLeftPx.x, scroller.mainRegion->rect.topLeftPx.y + scroller.mainRegion->rect.Height() * scroller.vScrollPosition), 
        NVec2f(scroller.mainRegion->rect.bottomRightPx.x, scroller.mainRegion->rect.topLeftPx.y + scroller.mainRegion->rect.Height() * scroller.vScrollPosition + thumbSize)),
        theme.GetColor(ThemeColor::WidgetActive));
}

}; // namespace Zep
