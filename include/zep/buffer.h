#pragma once

#include "editor.h"
#include "zep/line_widgets.h"
#include "zep/mcommon/file/path.h"
#include "theme.h"

#include "gap_buffer.h"
#include <set>

namespace Zep
{

class ZepSyntax;
class ZepTheme;
class ZepMode;
enum class ThemeColor;

enum class SearchDirection
{
    Forward,
    Backward
};

namespace SearchType
{
enum : uint32_t
{
    WORD = (1 << 0),
    Begin = (1 << 1),
    End = (1 << 2),
    Word = (1 << 3),
    SingleLine = (1 << 4)
};
};

namespace FileFlags
{
enum : uint32_t
{
    StrippedCR = (1 << 0),
    TerminatedWithZero = (1 << 1),
    ReadOnly = (1 << 2),
    Locked = (1 << 3), // Can this file path ever be written to?
    Dirty = (1 << 4), // Has the file been changed?
    HasWarnings = (1 << 6),
    HasErrors = (1 << 7),
    DefaultBuffer = (1 << 8) // Default startup buffer
};
}

// Ensure the character is >=0 and <=127 as in the ASCII standard,
// isalnum, for example will assert on debug build if not in this range.
inline int ToASCII(const char ch)
{
    auto ret = (unsigned int)ch;
    ret = std::min(ret, 127u);
    return ret;
}

enum class BufferType
{
    Normal,
    Search,
    Repl
};

enum class LineLocation
{
    None, // Not any specific location
    LineFirstGraphChar, // First non blank character
    LineLastGraphChar, // Last non blank character
    LineLastNonCR, // Last character before the carriage return
    LineBegin, // Beginning of line
    BeyondLineEnd, // The line end of the buffer line (for wrapped lines).
    LineCRBegin, // The first carriage return character
};

using BufferLocation = long;
struct BufferRange
{
    BufferLocation first;
    BufferLocation second;

    BufferRange(BufferLocation a, BufferLocation b)
        : first(a)
        , second(b)
    {
    }

    explicit BufferRange()
        : first(0)
        , second(0)
    {
    }

    bool ContainsLocation(BufferLocation loc) const
    {
        return loc >= first && loc < second;
    }
};

namespace RangeMarkerDisplayType
{
enum
{
    Underline = (1 << 0),           // Underline the range
    Background = (1 << 1),          // Add a background to the range
    Tooltip = (1 << 2),             // Show a tooltip using the name/description
    TooltipAtLine = (1 << 3),       // Tooltip shown if the user hovers the line
    CursorTip = (1 << 4),           // Tooltip shown if the user cursor is on the Mark
    CursorTipAtLine = (1 << 5),     // Tooltip shown if the user cursor is on the Mark line
    Indicator = (1 << 6),           // Show an indicator on the left side
    All = Underline | Tooltip | TooltipAtLine | CursorTip | CursorTipAtLine | Indicator | Background
};
};

enum class ToolTipPos
{
    AboveLine = 0,
    BelowLine = 1,
    RightLine = 2,
    Count = 3
};

struct RangeMarker
{
    long bufferLine = -1;
    BufferRange range;
    ThemeColor textColor = ThemeColor::Text;
    ThemeColor backgroundColor = ThemeColor::Background;
    ThemeColor highlightColor = ThemeColor::Background;
    uint32_t displayType = RangeMarkerDisplayType::All;
    std::string name;
    std::string description;
    ToolTipPos tipPos = ToolTipPos::AboveLine;

    bool ContainsLocation(long loc) const
    {
        return range.ContainsLocation(loc);
    }
    bool IntersectsRange(const BufferRange& i) const
    {
        return i.first < range.second && i.second > range.first;
    }
};

struct ZepRepl
{
    std::function<std::string(const std::string&)> fnParser;
    std::function<bool(const std::string&, int&)> fnIsFormComplete;
};

using tRangeMarkers = std::vector<std::shared_ptr<RangeMarker>>;

const long InvalidOffset = -1;

// A really big cursor move; which will likely clamp
static const long MaxCursorMove = long(0xFFFFFFF);

class ZepBuffer : public ZepComponent
{
public:
    ZepBuffer(ZepEditor& editor, const std::string& strName);
    ZepBuffer(ZepEditor& editor, const ZepPath& path);
    virtual ~ZepBuffer();

    void Clear();
    void SetText(const std::string& strText, bool initFromFile = false);
    void Load(const ZepPath& path);
    bool Save(int64_t& size);

    ZepPath GetFilePath() const;
    void SetFilePath(const ZepPath& path);

    BufferLocation Search(const std::string& str, BufferLocation start, SearchDirection dir = SearchDirection::Forward, BufferLocation end = BufferLocation{ -1l }) const;

    BufferLocation GetLinePos(BufferLocation bufferLocation, LineLocation lineLocation) const;
    bool GetLineOffsets(const long line, long& charStart, long& charEnd) const;
    BufferLocation Clamp(BufferLocation location) const;
    BufferLocation ClampToVisibleLine(BufferLocation in) const;
    long GetBufferColumn(BufferLocation location) const;
    bool InsideBuffer(BufferLocation location) const;
    using fnMatch = std::function<bool(const char)>;

    void Move(BufferLocation& loc, SearchDirection dir) const;
    bool Valid(BufferLocation locataion) const;
    bool MotionBegin(BufferLocation& start) const;
    bool Skip(fnMatch IsToken, BufferLocation& start, SearchDirection dir) const;
    bool SkipOne(fnMatch IsToken, BufferLocation& start, SearchDirection dir) const;
    bool SkipNot(fnMatch IsToken, BufferLocation& start, SearchDirection dir) const;

    BufferLocation FindOnLineMotion(BufferLocation start, const utf8* pCh, SearchDirection dir) const;
    BufferLocation WordMotion(BufferLocation start, uint32_t searchType, SearchDirection dir) const;
    BufferLocation EndWordMotion(BufferLocation start, uint32_t searchType, SearchDirection dir) const;
    BufferLocation ChangeWordMotion(BufferLocation start, uint32_t searchType, SearchDirection dir) const;
    BufferRange AWordMotion(BufferLocation start, uint32_t searchType) const;
    BufferRange InnerWordMotion(BufferLocation start, uint32_t searchType) const;
    BufferRange StandardCtrlMotion(BufferLocation cursor, SearchDirection searchDir) const;

    bool Delete(const BufferLocation& startOffset, const BufferLocation& endOffset);
    bool Insert(const BufferLocation& startOffset, const std::string& str);
    bool Replace(const BufferLocation& startOffset, const BufferLocation& endOffset, const std::string& str);

    long GetLineCount() const
    {
        return long(m_lineEnds.size());
    }
    long GetBufferLine(BufferLocation offset) const;
    BufferLocation LocationFromOffset(const BufferLocation& location, long offset) const;
    BufferLocation LocationFromOffset(long offset) const;
    BufferLocation LocationFromOffsetByChars(const BufferLocation& location, long offset, LineLocation loc = LineLocation::None) const;
    BufferLocation EndLocation() const;

    const GapBuffer<utf8>& GetText() const
    {
        return m_gapBuffer;
    }
    const std::vector<long> GetLineEnds() const
    {
        return m_lineEnds;
    }

    bool TestFlags(uint32_t flags)
    {
        return ((m_fileFlags & flags) == flags) ? true : false;
    }

    void ClearFlags(uint32_t flag)
    {
        SetFlags(flag, false);
    }

    void SetFlags(uint32_t flag, bool set = true)
    {
        if (set)
        {
            m_fileFlags |= flag;
        }
        else
        {
            m_fileFlags &= ~flag;
        }
    }
    void SetSyntaxProvider(SyntaxProvider provider)
    {
        if (provider.syntaxID != m_syntaxProvider.syntaxID)
        {
            m_spSyntax = provider.factory(this);
            m_syntaxProvider = provider;
        }
    }

    ZepSyntax* GetSyntax() const
    {
        return m_spSyntax.get();
    }

    const std::string& GetName() const
    {
        return m_strName;
    }

    std::string GetDisplayName() const;
    virtual void Notify(std::shared_ptr<ZepMessage> message) override;

    ZepTheme& GetTheme() const;
    void SetTheme(std::shared_ptr<ZepTheme> spTheme);

    void SetSelection(const BufferRange& sel);
    BufferRange GetSelection() const;

    void AddRangeMarker(std::shared_ptr<RangeMarker> spMarker);
    void ClearRangeMarkers();
    const tRangeMarkers& GetRangeMarkers() const;

    void SetBufferType(BufferType type);
    BufferType GetBufferType() const;

    void SetLastLocation(BufferLocation loc);
    BufferLocation GetLastLocation() const;

    ZepMode* GetMode() const;
    void SetMode(std::shared_ptr<ZepMode> spMode);

    void SetReplProvider(ZepRepl* repl) { m_replProvider = repl; }
    ZepRepl* GetReplProvider() const { return m_replProvider; }

    using tLineWidgets = std::vector<std::shared_ptr<ILineWidget>>;
    void AddLineWidget(long line, std::shared_ptr<ILineWidget> spWidget);
    void ClearLineWidgets(long line);
    const tLineWidgets* GetLineWidgets(long line) const;

    uint64_t GetLastUpdateTime() const { return m_lastUpdateTime; }
    uint64_t GetUpdateCount() const { return m_updateCount; }

    bool IsHidden() const;
private:
    // Internal
    GapBuffer<utf8>::const_iterator SearchWord(uint32_t searchType, GapBuffer<utf8>::const_iterator itrBegin, GapBuffer<utf8>::const_iterator itrEnd, SearchDirection dir) const;

    void MarkUpdate();

    void UpdateForInsert(const BufferLocation& startOffset, const BufferLocation& endOffset);
    void UpdateForDelete(const BufferLocation& startOffset, const BufferLocation& endOffset);

private:
    bool m_dirty = false; // Is the text modified?
    GapBuffer<utf8> m_gapBuffer; // Storage for the text - a gap buffer for efficiency
    std::vector<long> m_lineEnds; // End of each line
    uint32_t m_fileFlags = 0;
    BufferType m_bufferType = BufferType::Normal;
    std::shared_ptr<ZepSyntax> m_spSyntax;
    std::string m_strName;
    ZepPath m_filePath;
    std::shared_ptr<ZepTheme> m_spOverrideTheme;
    std::map<BufferLocation, std::vector<std::shared_ptr<ILineWidget>>> m_lineWidgets;

    BufferRange m_selection;
    tRangeMarkers m_rangeMarkers;
    BufferLocation m_lastLocation{ 0 };
    std::shared_ptr<ZepMode> m_spMode;
    ZepRepl* m_replProvider = nullptr; // May not be set
    SyntaxProvider m_syntaxProvider;
    uint64_t m_updateCount = 0;
    uint64_t m_lastUpdateTime = 0;
};

// Notification payload
enum class BufferMessageType
{
    // Inform clients that we are about to mess with the buffer
    PreBufferChange = 0,
    TextChanged,
    TextDeleted,
    TextAdded,
    Loaded
};

struct BufferMessage : public ZepMessage
{
    BufferMessage(ZepBuffer* pBuff, BufferMessageType messageType, const BufferLocation& startLoc, const BufferLocation& endLoc)
        : ZepMessage(Msg::Buffer)
        , pBuffer(pBuff)
        , type(messageType)
        , startLocation(startLoc)
        , endLocation(endLoc)
    {
    }

    ZepBuffer* pBuffer;
    BufferMessageType type;
    BufferLocation startLocation;
    BufferLocation endLocation;
};

} // namespace Zep
