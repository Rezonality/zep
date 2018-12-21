#pragma once

#include "buffer.h"
#include "window_base.h"

namespace Zep
{

class ZepTabWindow;

class IZepDisplay;

struct CharInfo
{
    float screenPosXPx;
    BufferLocation bufferLocation;
    BufferLocation bufferLocationEnd;
    NVec2f textSize;
};

// Line information, calculated during display update.
// A collection of spans that show split lines on the display
struct SpanInfo
{
    NVec2i columnOffsets; // Begin/end range of the text buffer for this line, as always end is one beyond the end.
    long lastNonCROffset = InvalidOffset; // The last char that is visible on the line (i.e. not CR/LF)
    float spanYPx; // Position in the buffer in pixels, if the screen was as big as the buffer.
    long bufferLineNumber = 0; // Line in the original buffer, not the screen line
    int lineIndex;

    std::vector<CharInfo> charInfo;

    long Length() const
    {
        return columnOffsets.y - columnOffsets.x;
    }
    bool BufferCursorInside(long offset) const
    {
        return offset >= columnOffsets.x && offset < columnOffsets.y;
    }
};

enum class CursorMode
{
    Hidden,
    Normal,
    Insert,
    Visual
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
    ShowCR = (1 << 1)
};
}

struct AirBox
{
    std::string text;
    uint32_t background;
};

struct Airline
{
    std::vector<AirBox> leftBoxes;
    std::vector<AirBox> rightBoxes;
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

    void SetCursorMode(CursorMode mode);
    void UpdateAirline();

    ZepTabWindow& GetTabWindow() const;

    void SetDisplayRegion(const DisplayRegion& region);

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

    void SetSelectionRange(BufferLocation start, BufferLocation end);

    ZepBuffer& GetBuffer() const;
    void SetBuffer(ZepBuffer* pBuffer);

    // Public for the tests!
    NVec2i BufferToDisplay();

    NVec2f GetTextSize(const utf8* pCh, const utf8* pEnd = nullptr);

    float ToWindowY(float pos) const;

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
    void CheckLineSpans();
    void ScrollToCursor();
    void UpdateVisibleLineRange();
    bool IsInsideTextRegion(NVec2i pos) const;

    const SpanInfo& GetCursorLineInfo(long y);

    NVec2i BufferToDisplay(const BufferLocation& location);

    bool DisplayLine(const SpanInfo& lineInfo, const DisplayRegion& region, int displayPass);

private:
    NVec2f ToBufferRegion(const NVec2f& pos);
    DisplayRegion m_bufferRegion; // region of the display we are showing on.
    DisplayRegion m_textRegion; // region of the display for text.
    DisplayRegion m_statusRegion; // status text / airline
    DisplayRegion m_leftRegion; // Numbers/indicators
    NVec2f m_topLeftPx; // Top-left position on screen
    NVec2f m_bottomRightPx; // Limits of the screen position
    bool m_wrap = true; // Wrap

    // The buffer offset is where we are looking, but the cursor is only what you see on the screen
    CursorMode m_cursorMode = CursorMode::Normal; // Type of cursor
    DisplayMode m_displayMode = DisplayMode::Vim; // Vim editing mode

    // Visual stuff
    std::vector<std::string> m_statusLines; // Status information, shown under the buffer

    float m_bufferOffsetYPx = 0;
    NVec2i m_visibleLineRange = { 0, 0 }; // Offset of the displayed area into the text
    std::vector<SpanInfo> m_windowLines; // Information about the currently displayed lines

    ZepTabWindow& m_tabWindow;

    uint32_t m_windowFlags = WindowFlags::ShowWhiteSpace;

    long m_maxDisplayLines = 0;
    float m_defaultLineSize = 0;

    BufferLocation m_bufferCursor{ 0 }; // Location in buffer coordinates.  Each window has a different buffer cursor
    long m_lastCursorColumn = 0; // The last cursor column (could be removed and recalculated)

    ZepBuffer* m_pBuffer = nullptr;
    Region m_selection; // Selection area

    bool m_linesChanged = true;

    Airline m_airline;
    std::map<Zep::utf8, NVec2f> m_mapCharSizes;
};

} // namespace Zep
