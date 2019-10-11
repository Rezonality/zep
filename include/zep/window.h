#pragma once

#include <vector>
#include <string>
#include <unordered_map>

#include "buffer.h"

namespace Zep
{

class ZepTabWindow;
class ZepDisplay;
class Scroller;

struct Region;

// Line information, calculated during display update.
// A collection of spans that show split lines on the display
struct SpanInfo
{
    BufferRange columnOffsets;                 // Begin/end range of the text buffer for this line, as always end is one beyond the end.
    long lastNonCROffset = InvalidOffset; // The last char that is visible on the line (i.e. not CR/LF)
    float spanYPx = 0.0f;                 // Position in the buffer in pixels, if the screen was as big as the buffer.
    float textHeight = 0.0f;              // Height of the text region in the line
    NVec2f margins = NVec2f(1.0f, 1.0f);  // Margin above and below the line
    long bufferLineNumber = 0;            // Line in the original buffer, not the screen line
    int lineIndex = 0;
    NVec2f pixelRenderRange;              // The x limits of where this line was last renderered

    float FullLineHeight() const
    {
        return margins.x + margins.y + textHeight;
    }

    long Length() const
    {
        return columnOffsets.second - columnOffsets.first;
    }

    bool BufferCursorInside(long offset) const
    {
        return offset >= columnOffsets.first && offset < columnOffsets.second;
    }
};

inline bool operator < (const SpanInfo& lhs, const SpanInfo& rhs)
{
    if (lhs.columnOffsets.first != rhs.columnOffsets.first)
    {
        return lhs.columnOffsets.first < rhs.columnOffsets.first;
    }
    return lhs.columnOffsets.second < rhs.columnOffsets.second;
}

enum class CursorType
{
    Hidden,
    Normal,
    Insert,
    Visual,
    LineMarker
};

enum class DisplayMode
{
    Normal,
    Vim
};

namespace WindowFlags
{
enum
{
    None = (0),
    ShowWhiteSpace = (1 << 0),
    ShowCR = (1 << 1),
    Modal
};
}

struct AirBox
{
    std::string text;
    NVec4f background;
};

struct Airline
{
    std::vector<AirBox> leftBoxes;
    std::vector<AirBox> rightBoxes;
};

// Message to request a tooltip.
// Clients should return the marker information if appropriate
struct ToolTipMessage : public ZepMessage
{
    ToolTipMessage(ZepBuffer* pBuff, const NVec2f& pos, const BufferLocation& loc = BufferLocation{-1})
        : ZepMessage(Msg::ToolTip, pos)
        , pBuffer(pBuff)
        , location(loc)
    {
    }

    ZepBuffer* pBuffer;
    BufferLocation location;
    std::shared_ptr<RangeMarker> spMarker;
};

// Display state for a single pane of text.
// Window shows a buffer, and is parented by a TabWindow
// The buffer can change, but the window must always have an active buffer
// Editor operations such as select and change are local to a displayed pane
class ZepWindow : public ZepComponent
{
public:
    ZepWindow(ZepTabWindow& window, ZepBuffer* buffer);
    virtual ~ZepWindow();

    virtual void Notify(std::shared_ptr<ZepMessage> message) override;

    void SetCursorType(CursorType mode);
    void UpdateAirline();
    void UpdateScrollers();

    ZepTabWindow& GetTabWindow() const;

    void SetDisplayRegion(const NRectf& region);

    void Display();
    void DisplayCursor();

    void MoveCursorY(int yDistance, LineLocation clampLocation = LineLocation::LineLastNonCR);

    BufferLocation GetBufferCursor();
    void SetBufferCursor(BufferLocation location);

    // Flags
    void SetWindowFlags(uint32_t windowFlags);
    uint32_t GetWindowFlags() const;
    void ToggleFlag(uint32_t flag);

    long GetMaxDisplayLines();
    long GetNumDisplayedLines();

    ZepBuffer& GetBuffer() const;
    void SetBuffer(ZepBuffer* pBuffer);

    NVec2i BufferToDisplay();
    NVec2i BufferToDisplay(const BufferLocation& location);

    float ToWindowY(float pos) const;

    bool IsActiveWindow() const;
    NVec4f FilterActiveColor(const NVec4f& col, float atten = 1.0f);

    void UpdateLayout(bool force = false);

    enum FitCriteria
    {
        X,
        Y
    };
    bool RectFits(const NRectf& area, const NRectf& rect, FitCriteria criteria);

private:
    struct WindowPass
    {
        enum Pass
        {
            Background = 0,
            Text,
            Max
        };
    };

private:
    void UpdateLineSpans();
    void ScrollToCursor();
    void EnsureCursorVisible();
    void UpdateVisibleLineRange();
    bool IsInsideTextRegion(NVec2i pos) const;

    void GetCharPointer(BufferLocation loc, const utf8*& pBegin, const utf8*& pEnd, bool& invalidChar);
    const SpanInfo& GetCursorLineInfo(long y);

    float TipBoxShadowWidth() const;
    void DisplayToolTip(const NVec2f& pos, const RangeMarker& marker) const;
    bool DisplayLine(SpanInfo& lineInfo, int displayPass);
    void DisplayScrollers();
    void DisableToolTipTillMove();

    NVec4f GetBlendedColor(ThemeColor color) const;
    void GetCursorInfo(NVec2f& pos, NVec2f& size);

    void PlaceToolTip(const NVec2f& pos, ToolTipPos location, uint32_t lineGap, const std::shared_ptr<RangeMarker> spMarker);
private:
    NVec2f ToBufferRegion(const NVec2f& pos);
    std::shared_ptr<Region> m_bufferRegion;  // region of the display we are showing on.
    std::shared_ptr<Region> m_textRegion;    // region of the display for text.
    std::shared_ptr<Region> m_airlineRegion; // Airline
    std::shared_ptr<Region> m_numberRegion;     // Numbers
    std::shared_ptr<Region> m_indicatorRegion;  // Indicators 
    std::shared_ptr<Region> m_vScrollRegion;    // Vertical scroller

    // Wrap ; need horizontal offset for this to be turned on.
    // This will indeed stop lines wrapping though!  You just can't move to the far right and have
    // the text scroll; which isn't a big deal, but a work item.
    bool m_wrap = true;     

    // The buffer offset is where we are looking, but the cursor is only what you see on the screen
    CursorType m_cursorType = CursorType::Normal; // Type of cursor
    DisplayMode m_displayMode = DisplayMode::Vim; // Vim editing mode

    // Visual stuff
    std::vector<std::string> m_statusLines; // Status information, shown under the buffer

    float m_bufferOffsetYPx = 0.0f;
    float m_bufferSizeYPx = 0.0f;
    NVec2i m_visibleLineRange = {0, 0};  // Offset of the displayed area into the text

    std::vector<SpanInfo*> m_windowLines; // Information about the currently displayed lines

    ZepTabWindow& m_tabWindow;

    uint32_t m_windowFlags = WindowFlags::ShowWhiteSpace;

    long m_maxDisplayLines = 0;
    float m_defaultLineSize = 0;

    BufferLocation m_bufferCursor{0}; // Location in buffer coordinates.  Each window has a different buffer cursor
    long m_lastCursorColumn = 0;      // The last cursor column (could be removed and recalculated)

    ZepBuffer* m_pBuffer = nullptr;
    Airline m_airline;

    bool m_layoutDirty = true;
    bool m_scrollVisibilityChanged = true;
    bool m_cursorMoved = true;

    NVec2f m_visibleLineExtents;            // The pixel extents of all the lines

    std::shared_ptr<Scroller> m_vScroller;
    timer m_toolTipTimer;                   // Timer for when the tip is shown
    NVec2f m_mouseHoverPos;                   // Current location for the tip
    NVec2f m_lastTipQueryPos;               // last query location for the tip
    bool m_tipDisabledTillMove = false;     // Certain operations will stop the tip until the mouse is moved
    BufferLocation m_mouseBufferLocation;     // The character in the buffer the tip pos is over, or -1
    std::map<NVec2f, std::shared_ptr<RangeMarker>> m_toolTips;  // All tooltips for a given position, currently only 1 at a time

};

} // namespace Zep
