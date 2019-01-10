#pragma once

#include "splits.h"
#include "editor.h"

namespace Zep
{
class ZepTheme;
class ZepEditor;

struct Scroller : public ZepComponent
{
public:
    Scroller(ZepEditor& editor, Region& parent);
    
    virtual void Display(ZepTheme& theme);
    virtual void Notify(std::shared_ptr<ZepMessage> message) override;

    float vScrollVisiblePercent = 1.0f;
    float vScrollPosition = 0.0f;
    float vScrollLinePercent = 0.0f;
    float vScrollPagePercent = 0.0f;
    bool vertical = true;

private:
    void ClickUp();
    void ClickDown();

private:
    std::shared_ptr<Region> region;
    std::shared_ptr<Region> topButtonRegion;
    std::shared_ptr<Region> bottomButtonRegion;
    std::shared_ptr<Region> mainRegion;
};

};
