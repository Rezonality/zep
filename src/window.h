#pragma once

#include "buffer.h"
#include "window_base.h"

namespace Zep
{

class ZepDisplay;
class ZepTabWindow;

// Line information, calculated during display update.
// This is a screen line, not a text buffer line, since we may wrap across multiple lines
struct LineInfo
{
    NVec2i columnOffsets; // Begin/end range of the text buffer for this line, as always end is one beyond the end.
    long lastNonCROffset = InvalidOffset; // The last char that is visible on the line (i.e. not CR/LF)
    float screenPosYPx; // Current position on Screen
    long bufferLineNumber = 0; // Line in the original buffer, not the screen line
    long screenLineNumber = 0; // Line on the screen

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

// Display state for a single pane of text.
// Window shows a buffer, and is parented by a TabWindow
// The buffer can change, but the window must always have an active buffer
// Editor operations such as select and change are local to a displayed pane
class ZepWindow : public ZepWindowBase
{
public:
    ZepWindow(ZepTabWindow& window, ZepBuffer* buffer);
    virtual ~ZepWindow();

    virtual void Notify(std::shared_ptr<ZepMessage> message) override;

    void SetCursorMode(CursorMode mode);
    void SetStatusText(const std::string& strStatus);

    ZepTabWindow& GetTabWindow() const;

    struct WindowPass
    {
        enum Pass
        {
            Background = 0,
            Text,
            Max
        };
    };

    void PreDisplay(ZepDisplay& display, const DisplayRegion& region);
    void Display(ZepDisplay& display);
    bool DisplayLine(ZepDisplay& display, const LineInfo& lineInfo, const DisplayRegion& region, int displayPass);

    void SetWindowFlags(uint32_t windowFlags)
    {
        m_windowFlags = windowFlags;
    }
    uint32_t GetWindowFlags() const
    {
        return m_windowFlags;
    }

    long GetMaxDisplayLines() const
    {
        return m_maxDisplayLines;
    }

    void UpdateVisibleLineData();
    void UpdateScreenLines();
    long VisibleLineCount() const
    {
        return visibleLineRange.y - visibleLineRange.x;
    }
    const LineInfo& GetCursorLineInfo(long y) const;

    void ScrollToCursor();

public:
    // TODO: Fix this; used to be a struct, now members
    // Should be private!
    DisplayRegion m_bufferRegion; // region of the display we are showing on.
    DisplayRegion m_textRegion; // region of the display for text.
    DisplayRegion m_statusRegion; // status text / airline
    DisplayRegion m_leftRegion; // Numbers/indicators
    NVec2f topLeftPx; // Top-left position on screen
    NVec2f bottomRightPx; // Limits of the screen position
    bool wrap = true; // Wrap

    // The buffer offset is where we are looking, but the cursor is only what you see on the screen
    CursorMode cursorMode = CursorMode::Normal; // Type of cursor
    DisplayMode displayMode = DisplayMode::Vim; // Vim editing mode

    // Visual stuff
    std::vector<std::string> statusLines; // Status information, shown under the buffer

    NVec2i visibleLineRange = { 0, 0 }; // Offset of the displayed area into the text
    std::vector<LineInfo> windowLines; // Information about the currently displayed lines
    bool m_pendingLineUpdate = true;
    bool m_linesFillScreen = false;

    ZepTabWindow& m_tabWindow;

    uint32_t m_windowFlags = WindowFlags::ShowWhiteSpace;

    long m_maxDisplayLines = 0;
    float m_defaultLineSize = 0;

private:
    BufferLocation m_bufferCursor{ 0 }; // Location in buffer coordinates.  Each window has a different buffer cursor
};

} // namespace Zep
