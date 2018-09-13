#pragma once

#include "buffer.h"

namespace Zep
{

class ZepTabWindow;
class Timer;
const float bottomBorder = 4.0f;
const float textBorder = 4.0f;
const float leftBorder = 30.0f;

struct DisplayRegion
{
    NVec2f topLeftPx;
    NVec2f bottomRightPx;
    NVec2f BottomLeft() const { return NVec2f(topLeftPx.x, bottomRightPx.y); }
    NVec2f TopRight() const { return NVec2f(bottomRightPx.x, topLeftPx.y); }
    float Height() const { return bottomRightPx.y - topLeftPx.y; }
};

// A region inside the text for selections
struct Region
{
    NVec2i startCL;     // Display Line/Column
    NVec2i endCL;
    bool visible;
    bool vertical;      // Not yet supported
};

class ZepDisplay : public ZepComponent
{
public:
    using tWindows = std::set<std::shared_ptr<ZepTabWindow>>;

    ZepDisplay(ZepEditor& editor);
    virtual ~ZepDisplay();

    void SetDisplaySize(const NVec2f& topLeft, const NVec2f& bottomRight);

    void PreDisplay();
    void Display();

    virtual void Notify(std::shared_ptr<ZepMessage> spMsg);

    // Renderer specific overrides
    // Implement these to draw the buffer using whichever system you prefer
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const = 0;
    virtual float GetFontSize() const = 0;
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, uint32_t color = 0xFFFFFFFF, float width = 1.0f) const = 0;
    virtual void DrawChars(const NVec2f& pos, uint32_t col, const utf8* text_begin, const utf8* text_end = nullptr) const = 0;
    virtual void DrawRectFilled(const NVec2f& a, const NVec2f& b, uint32_t col = 0xFFFFFFFF) const = 0;

    void SetCommandText(const std::string& strCommand);

    // Setup display for any window if there is none
    void AssignDefaultWindow();

    // Window management
    void SetCurrentWindow(ZepTabWindow* pTabWindow);
    ZepTabWindow* GetCurrentWindow() const;
    ZepTabWindow* AddWindow();
    void RemoveWindow(ZepTabWindow* pTabWindow);
    const tWindows& GetWindows() const;

    void RequestRefresh();
    bool RefreshRequired() const;

    bool GetCursorBlinkState() const;
    void ResetCursorTimer();

protected:
    void DrawRegion(const Region& region);

protected:
    // TODO: A splitter manager
    DisplayRegion m_tabContentRegion;
    DisplayRegion m_commandRegion;
    DisplayRegion m_tabRegion;

    NVec2f m_topLeftPx;
    NVec2f m_bottomRightPx;

    // Blinking cursor
    std::shared_ptr<Timer> m_spCursorTimer;
    mutable bool m_bPendingRefresh = true;
    mutable bool m_lastCursorBlink = false;

    // One window for now
    tWindows m_windows;
    ZepTabWindow* m_pCurrentTabWindow = nullptr;

    std::vector<std::string> m_commandLines;        // Command information, shown under the buffer
};

// A NULL renderer, used for testing
// Discards all drawing, and returns text size of 1 pixel per char!
// This is the only work you need to do to make a new renderer, other than ImGui
class ZepDisplayNull : public ZepDisplay
{
public:
    ZepDisplayNull(ZepEditor& editor)
        : ZepDisplay(editor)
    {

    }
    virtual NVec2f GetTextSize(const utf8* pBegin, const utf8* pEnd = nullptr) const { return NVec2f(float(pEnd - pBegin), 10.0f); }
    virtual float GetFontSize() const { return 10; };
    virtual void DrawLine(const NVec2f& start, const NVec2f& end, uint32_t color = 0xFFFFFFFF, float width = 1.0f) const { };
    virtual void DrawChars(const NVec2f& pos, uint32_t col, const utf8* text_begin, const utf8* text_end = nullptr) const { };
    virtual void DrawRectFilled(const NVec2f& a, const NVec2f& b, uint32_t col = 0xFFFFFFFF) const {};
};

} // Zep
