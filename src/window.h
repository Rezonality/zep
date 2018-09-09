#pragma once

#include "buffer.h"

namespace Zep
{

class ZepDisplay;

struct DisplayRegion
{
    NVec2f topLeftPx;
    NVec2f bottomRightPx;
    NVec2f BottomLeft() const { return NVec2f(topLeftPx.x, bottomRightPx.y); }
    NVec2f TopRight() const { return NVec2f(bottomRightPx.x, topLeftPx.y); }
    float Height() const { return bottomRightPx.y - topLeftPx.y; }
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
// A region inside the text for selections
struct Region
{
    NVec2i startCL;     // Display Line/Column
    NVec2i endCL;
    bool visible;
    bool vertical;      // Not yet supported
};

// A really big cursor move; which will likely clamp
static const long MaxCursorMove = long(0xFFFFFFF);

// Line information, calculated during display update.
// This is a screen line, not a text buffer line, since we may wrap across multiple lines
struct LineInfo
{
    NVec2i columnOffsets;                        // Begin/end range of the text buffer for this line, as always end is one beyond the end.
    long lastNonCROffset = InvalidOffset;        // The last char that is visible on the line (i.e. not CR/LF)
    long firstGraphCharOffset = InvalidOffset;   // First graphic char
    long lastGraphCharOffset = InvalidOffset;    // Last graphic char
    float screenPosYPx;                          // Current position on Screen
    long lineNumber = 0;                         // Line in the original buffer, not the screen line
    long screenLineNumber = 0;                   // Line on the screen

    long Length() const { return columnOffsets.y - columnOffsets.x; }
};


class ZepSyntax;

// Display state for a single pane of text.
// Editor operations such as select and change are local to a displayed pane
class ZepWindow : public ZepComponent
{
public:
    using tBuffers = std::set<ZepBuffer*>;

    ZepWindow(ZepDisplay& display);
    virtual ~ZepWindow();

    virtual void Notify(std::shared_ptr<ZepMessage> message) override;

    void PreDisplay(const DisplayRegion& region);

    void SetCursorMode(CursorMode mode);
    void SetSyntax(std::shared_ptr<ZepSyntax> syntax) { m_spSyntax = syntax; }

    // Convert cursor to buffer location
    BufferLocation DisplayToBuffer() const;
    BufferLocation DisplayToBuffer(const NVec2i& display) const;

    void MoveCursorTo(const BufferLocation& location, LineLocation clampLocation = LineLocation::LineLastNonCR);

    void MoveCursor(LineLocation location);
    void MoveCursor(const NVec2i& distance, LineLocation clampLocation = LineLocation::LineLastNonCR);

    // Convert buffer to cursor offset
    NVec2i BufferToDisplay(const BufferLocation& location) const;
    
    void ClampCursorToDisplay();
    long ClampVisibleLine(long line) const;
    NVec2i ClampVisibleColumn(NVec2i location, LineLocation loc) const;

    void SetSelectionRange(const NVec2i& start, const NVec2i& end);
    void SetStatusText(const std::string& strStatus);

    void SetCurrentBuffer(ZepBuffer* pBuffer);
    ZepBuffer* GetCurrentBuffer() const;
    void AddBuffer(ZepBuffer* pBuffer);
    void RemoveBuffer(ZepBuffer* spBuffer);
    const tBuffers& GetBuffers() const;

    const NVec2i& GetCursor() const { return cursorCL; }
    void SetCursor(const NVec2i& cursor) { cursorCL = cursor;}

    ZepDisplay& GetDisplay() const { return m_display; }

    struct WindowPass
    {
        enum Pass
        {
            Background = 0,
            Text,
            Max 
        };
    };
    void Display();
    bool DisplayLine(const LineInfo& lineInfo, const DisplayRegion& region, int displayPass);

    void SetWindowFlags(uint32_t windowFlags) { m_windowFlags = windowFlags; }
    uint32_t GetWindowFlags() const { return m_windowFlags;  }

    long GetMaxDisplayLines() const { return m_maxDisplayLines; }

public:
    // TODO: Fix this; used to be a struct, now members
    // Should be private!
    ZepDisplay& m_display;                     // Display that owns this window
    DisplayRegion m_windowRegion;                 // region of the display we are showing on.
    DisplayRegion m_textRegion;                   // region of the display for text.
    DisplayRegion m_tabRegion;                    // tab area
    DisplayRegion m_statusRegion;                 // status text / airline
    DisplayRegion m_leftRegion;                   // Numbers/indicators
    NVec2f topLeftPx;                             // Top-left position on screen
    NVec2f bottomRightPx;                         // Limits of the screen position
    NVec2f cursorPosPx;                           // Cursor location
    bool wrap = true;                             // Wrap
    bool tabs = true;                             // Show tabs

    // The buffer offset is where we are looking, but the cursor is only what you see on the screen
    CursorMode cursorMode = CursorMode::Normal;   // Type of cursor
    DisplayMode displayMode = DisplayMode::Vim;   // Vim editing mode
    long lastCursorC = 0;                         // The last cursor column

    NVec2i bufferCL;                              // Offset of the displayed area into the text

    Region selection;                             // Selection area

    // Visual stuff
    std::vector<std::string> statusLines;         // Status information, shown under the buffer
    std::vector<LineInfo> visibleLines;           // Information about the currently displayed lines 

    static const int CursorMax = std::numeric_limits<int>::max();

    std::shared_ptr<ZepSyntax> m_spSyntax;

    tBuffers m_buffers;
    ZepBuffer* m_pCurrentBuffer = nullptr;

    uint32_t m_windowFlags = WindowFlags::None;

    long m_maxDisplayLines = 0;

private:
    NVec2i cursorCL;                              // Position of Cursor in line/column (display coords)
};

} // Zep
