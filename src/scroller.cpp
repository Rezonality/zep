#include "common_namespace.h"
#include "editor.h"
#include "theme.h"
#include "display.h"
#include "logger.h"
#include "scroller.h"

namespace Zep
{

Scroller::Scroller(ZepEditor& editor, Region& parent)
    : ZepComponent(editor)
{
    region = std::make_shared<Region>();
    topButtonRegion = std::make_shared<Region>();
    bottomButtonRegion = std::make_shared<Region>();
    mainRegion = std::make_shared<Region>();

    region->flags = RegionFlags::Expanding;
    topButtonRegion->flags = RegionFlags::Fixed;
    bottomButtonRegion->flags = RegionFlags::Fixed;
    mainRegion->flags = RegionFlags::Expanding;

    region->vertical = false;

    const float scrollButtonMargin = 4.0f * editor.GetPixelScale();
    topButtonRegion->margin = NVec2f(scrollButtonMargin, scrollButtonMargin);
    bottomButtonRegion->margin = NVec2f(scrollButtonMargin, scrollButtonMargin);
    mainRegion->margin = NVec2f(scrollButtonMargin, 0.0f);

    const float scrollButtonSize = 16.0f * editor.GetPixelScale();
    topButtonRegion->fixed_size = NVec2f(0.0f, scrollButtonSize);
    bottomButtonRegion->fixed_size = NVec2f(0.0f, scrollButtonSize);

    region->children.push_back(topButtonRegion);
    region->children.push_back(mainRegion);
    region->children.push_back(bottomButtonRegion);

    parent.children.push_back(region);
}

void Scroller::ClickUp()
{
    vScrollPosition -= vScrollLinePercent;
    vScrollPosition = std::max(0.0f, vScrollPosition);
    GetEditor().Broadcast(std::make_shared<ZepMessage>(Msg::ComponentChanged, this));
    
}

void Scroller::ClickDown()
{
    vScrollPosition += vScrollLinePercent;
    vScrollPosition = std::min(1.0f - vScrollVisiblePercent, vScrollPosition);
    GetEditor().Broadcast(std::make_shared<ZepMessage>(Msg::ComponentChanged, this));
}

void Scroller::Notify(std::shared_ptr<ZepMessage> message)
{
    switch (message->messageId)
    {
    case Msg::MouseDown:
        if (message->button == ZepMouseButton::Left)
        {
            if (bottomButtonRegion->rect.Contains(message->pos))
            {
                ClickDown();
            }
        }
        break;
    case Msg::MouseUp:
        if (message->button == ZepMouseButton::Left)
        {
            if (topButtonRegion->rect.Contains(message->pos))
            {
                ClickUp();
            }
        }
        break;
    case Msg::MouseMove:
        break;
    default:
        break;
    }
}

void Scroller::Display(ZepTheme& theme)
{
    auto& display = GetEditor().GetDisplay();

    display.SetClipRect(region->rect);

    float thumbSize = mainRegion->rect.Height() * vScrollVisiblePercent;

    auto mousePos = GetEditor().GetMousePos();
    auto activeColor = theme.GetColor(ThemeColor::WidgetActive);
    auto inactiveColor = theme.GetColor(ThemeColor::WidgetInactive);

    // Scroller background
    display.DrawRectFilled(region->rect, theme.GetColor(ThemeColor::WidgetBackground));

    display.DrawRectFilled(topButtonRegion->rect, topButtonRegion->rect.Contains(mousePos) ? activeColor : inactiveColor);
    display.DrawRectFilled(bottomButtonRegion->rect, bottomButtonRegion->rect.Contains(mousePos) ? activeColor : inactiveColor);

    NRectf thumbRect(NVec2f(mainRegion->rect.topLeftPx.x, mainRegion->rect.topLeftPx.y + mainRegion->rect.Height() * vScrollPosition),
        NVec2f(mainRegion->rect.bottomRightPx.x, mainRegion->rect.topLeftPx.y + mainRegion->rect.Height() * vScrollPosition + thumbSize));

    // Thumb
    display.DrawRectFilled(thumbRect, thumbRect.Contains(mousePos) ? activeColor : inactiveColor);
}

}; // namespace Zep
