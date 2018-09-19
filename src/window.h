#pragma once

#include "buffer.h"

namespace Zep
{

class ZepDisplay;
class ZepTabWindow;
class ZepSyntax;

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

class ZepSyntax;

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
    void SetSyntax(std::shared_ptr<ZepSyntax> syntax) { m_spSyntax = syntax; }

    // Convert cursor to buffer location
    BufferLocation DisplayToBuffer() const;
    BufferLocation DisplayToBuffer(const NVec2i& display) const;

    void MoveCursor(BufferLocation location);
    void MoveCursor(LineLocation location);

    void MoveCursor(const NVec2i& distance, LineLocation clampLocation = LineLocation::LineLastNonCR);

    // Convert buffer to cursor offset
    NVec2i BufferToDisplay(const BufferLocation& location) const;
    NVec2i BufferToDisplay() const;
    
    void ClampCursorToDisplay();
    long ClampVisibleLine(long line) const;
    NVec2i ClampVisibleColumn(NVec2i location, LineLocation loc) const;

    void SetSelectionRange(const NVec2i& start, const NVec2i& end);
    void SetStatusText(const std::string& strStatus);

    BufferLocation GetBufferLocation() const { return m_bufferLocation; }
    void SetBufferLocation(BufferLocation loc) { m_bufferLocation = loc;}

    ZepBuffer& GetBuffer() const { return *m_pBuffer; }
    ZepTabWindow& GetTabWindow() const { return m_window; }

    void SetBuffer(ZepBuffer* pBuffer);

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

    void SetWindowFlags(uint32_t windowFlags) { m_windowFlags = windowFlags; }
    uint32_t GetWindowFlags() const { return m_windowFlags;  }

    long GetMaxDisplayLines() const { return m_maxDisplayLines; }

public:
    // TODO: Fix this; used to be a struct, now members
    // Should be private!
    DisplayRegion m_bufferRegion;                 // region of the display we are showing on.
    DisplayRegion m_textRegion;                   // region of the display for text.
    DisplayRegion m_statusRegion;                 // status text / airline
    DisplayRegion m_leftRegion;                   // Numbers/indicators
    NVec2f topLeftPx;                             // Top-left position on screen
    NVec2f bottomRightPx;                         // Limits of the screen position
    NVec2f cursorPosPx;                           // Cursor location
    bool wrap = true;                             // Wrap

    // The buffer offset is where we are looking, but the cursor is only what you see on the screen
    CursorMode cursorMode = CursorMode::Normal;   // Type of cursor
    DisplayMode displayMode = DisplayMode::Vim;   // Vim editing mode
    long lastCursorC = 0;                         // The last cursor column

    long bufferLineOffset = 0;                    // Offset of the displayed area into the text
    NVec2i cursorCL;                              // Position of Cursor in line/column (display coords)
    BufferLocation m_bufferLocation{ 0 };            // Location in buffer coordinates

    Region selection;                             // Selection area

    // Visual stuff
    std::vector<std::string> statusLines;         // Status information, shown under the buffer
    std::vector<LineInfo> visibleLines;           // Information about the currently displayed lines 

    static const int CursorMax = std::numeric_limits<int>::max();

    std::shared_ptr<ZepSyntax> m_spSyntax;

    ZepBuffer* m_pBuffer;
    ZepTabWindow& m_window;

    uint32_t m_windowFlags = WindowFlags::None;

    long m_maxDisplayLines = 0;

private:
};

} // Zep
