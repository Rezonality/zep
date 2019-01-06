#include "common_namespace.h"
#include "editor.h"
#include "theme.h"
#include "scroller.h"
#include "display.h"
#include "logger.h"

namespace Zep
{


void Scroller_Init(Scroller& scroller, ZepEditor& editor, Region& parent)
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

    const float scrollButtonMargin = 4.0f * editor.GetPixelScale();
    scroller.topButtonRegion->margin = NVec2f(scrollButtonMargin, scrollButtonMargin);
    scroller.bottomButtonRegion->margin = NVec2f(scrollButtonMargin, scrollButtonMargin);
    scroller.mainRegion->margin = NVec2f(scrollButtonMargin, 0.0f);

    const float scrollButtonSize = 16.0f * editor.GetPixelScale();
    scroller.topButtonRegion->fixed_size = NVec2f(0.0f, scrollButtonSize);
    scroller.bottomButtonRegion->fixed_size = NVec2f(0.0f, scrollButtonSize);

    scroller.region->children.push_back(scroller.topButtonRegion);
    scroller.region->children.push_back(scroller.mainRegion);
    scroller.region->children.push_back(scroller.bottomButtonRegion);

    parent.children.push_back(scroller.region);
}

void Scroller_Display(Scroller& scroller, ZepEditor& editor, ZepTheme& theme)
{
    auto& display = editor.GetDisplay();

    display.SetClipRect(scroller.region->rect);

    float thumbSize = scroller.mainRegion->rect.Height() * scroller.vScrollVisiblePercent;

    auto mousePos = editor.GetMousePos();
    auto activeColor = theme.GetColor(ThemeColor::WidgetActive);
    auto inactiveColor = theme.GetColor(ThemeColor::WidgetInactive);

    // Scroller background
    display.DrawRectFilled(scroller.region->rect, theme.GetColor(ThemeColor::WidgetBackground));

    display.DrawRectFilled(scroller.topButtonRegion->rect, scroller.topButtonRegion->rect.Contains(mousePos) ? activeColor : inactiveColor);
    display.DrawRectFilled(scroller.bottomButtonRegion->rect, scroller.bottomButtonRegion->rect.Contains(mousePos) ? activeColor : inactiveColor);

    NRectf thumbRect(NVec2f(scroller.mainRegion->rect.topLeftPx.x, scroller.mainRegion->rect.topLeftPx.y + scroller.mainRegion->rect.Height() * scroller.vScrollPosition),
        NVec2f(scroller.mainRegion->rect.bottomRightPx.x, scroller.mainRegion->rect.topLeftPx.y + scroller.mainRegion->rect.Height() * scroller.vScrollPosition + thumbSize));

    // Thumb
    display.DrawRectFilled(thumbRect, thumbRect.Contains(mousePos) ? activeColor : inactiveColor);
}

}; // namespace Zep
