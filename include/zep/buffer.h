#pragma once

#include <functional>
#include <set>

#include "zep/mcommon/file/path.h"
#include "zep/mcommon/utf8/unchecked.h"
#include "zep/mcommon/string/stringutils.h"
#include "zep/mcommon/logger.h"

#include "gap_buffer.h"

#include "editor.h"
#include "line_widgets.h"
#include "theme.h"

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
    Repl,
    DataGrid,
    Tree
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

using ByteIndex = long;
struct BufferByteRange
{
    ByteIndex first;
    ByteIndex second;

    BufferByteRange(ByteIndex a, ByteIndex b)
        : first(a)
        , second(b)
    {
    }

    explicit BufferByteRange()
        : first(0)
        , second(0)
    {
    }

    bool ContainsLocation(ByteIndex loc) const
    {
        return loc >= first && loc < second;
    }
};

namespace RangeMarkerDisplayType
{
enum
{
    Hidden = 0,
    Underline = (1 << 0), // Underline the range
    Background = (1 << 1), // Add a background to the range
    Tooltip = (1 << 2), // Show a tooltip using the name/description
    TooltipAtLine = (1 << 3), // Tooltip shown if the user hovers the line
    CursorTip = (1 << 4), // Tooltip shown if the user cursor is on the Mark
    CursorTipAtLine = (1 << 5), // Tooltip shown if the user cursor is on the Mark line
    Indicator = (1 << 6), // Show an indicator on the left side
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

namespace RangeMarkerType
{
enum
{
    Message = (1 << 0),
    Search = (1 << 1),
    All = (Message | Search)
};
};

struct RangeMarker
{
    BufferByteRange range;
    ThemeColor textColor = ThemeColor::Text;
    ThemeColor backgroundColor = ThemeColor::Background;
    ThemeColor highlightColor = ThemeColor::Background;
    uint32_t displayType = RangeMarkerDisplayType::All;
    uint32_t markerType = RangeMarkerType::Message;
    std::string name;
    std::string description;
    ToolTipPos tipPos = ToolTipPos::AboveLine;

    bool ContainsLocation(ByteIndex loc) const
    {
        return range.ContainsLocation(loc);
    }
    bool IntersectsRange(const BufferByteRange& i) const
    {
        return i.first < range.second && i.second > range.first;
    }
};

using tRangeMarkers = std::map<ByteIndex, std::set<std::shared_ptr<RangeMarker>>>;

const ByteIndex InvalidByteIndex = -1;

// A really big cursor move; which will likely clamp
static const ByteIndex MaxCursorMove = ByteIndex(0xFFFFFFF);

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
    std::string GetFileExtension() const;
    void SetFilePath(const ZepPath& path);

    ByteIndex GetLinePos(ByteIndex bufferLocation, LineLocation lineLocation) const;
    bool GetLineOffsets(const long line, ByteIndex& charStart, ByteIndex& charEnd) const;
    ByteIndex Clamp(ByteIndex location) const;
    ByteIndex ClampToVisibleLine(ByteIndex in) const;
    long GetBufferColumn(ByteIndex location) const;
    bool InsideBuffer(ByteIndex location) const;
    using fnMatch = std::function<bool(const char)>;

    void Move(ByteIndex& loc, SearchDirection dir) const;
    bool Valid(ByteIndex locataion) const;
    bool MotionBegin(ByteIndex& start) const;
    bool Skip(fnMatch IsToken, ByteIndex& start, SearchDirection dir) const;
    bool SkipOne(fnMatch IsToken, ByteIndex& start, SearchDirection dir) const;
    bool SkipNot(fnMatch IsToken, ByteIndex& start, SearchDirection dir) const;

    ByteIndex Find(ByteIndex start, const uint8_t* pBegin, const uint8_t* pEnd) const;
    ByteIndex FindOnLineMotion(ByteIndex start, const uint8_t* pCh, SearchDirection dir) const;
    ByteIndex WordMotion(ByteIndex start, uint32_t searchType, SearchDirection dir) const;
    ByteIndex EndWordMotion(ByteIndex start, uint32_t searchType, SearchDirection dir) const;
    ByteIndex ChangeWordMotion(ByteIndex start, uint32_t searchType, SearchDirection dir) const;
    BufferByteRange AWordMotion(ByteIndex start, uint32_t searchType) const;
    BufferByteRange InnerWordMotion(ByteIndex start, uint32_t searchType) const;
    BufferByteRange StandardCtrlMotion(ByteIndex cursor, SearchDirection searchDir) const;

    bool Delete(const ByteIndex& startOffset, const ByteIndex& endOffset);
    bool Insert(const ByteIndex& startOffset, const std::string& str);
    bool Replace(const ByteIndex& startOffset, const ByteIndex& endOffset, const std::string& str);

    long GetLineCount() const
    {
        return long(m_lineEnds.size());
    }
    long GetBufferLine(ByteIndex offset) const;

    ByteIndex EndLocation() const;

    const GapBuffer<uint8_t>& GetText() const
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

    void SetSelection(const BufferByteRange& sel);
    BufferByteRange GetSelection() const;
    bool HasSelection() const;
    void ClearSelection();

    void AddRangeMarker(std::shared_ptr<RangeMarker> spMarker);
    void ClearRangeMarkers(const std::set<std::shared_ptr<RangeMarker>>& markers);
    void ClearRangeMarkers(uint32_t types);
    tRangeMarkers GetRangeMarkers(uint32_t types) const;
    void HideMarkers(uint32_t markerType);
    void ShowMarkers(uint32_t markerType, uint32_t displayType);

    void ForEachMarker(uint32_t types, SearchDirection dir, ByteIndex begin, ByteIndex end, std::function<bool(const std::shared_ptr<RangeMarker>&)> fnCB) const;
    std::shared_ptr<RangeMarker> FindNextMarker(ByteIndex start, SearchDirection dir, uint32_t markerType);

    void SetBufferType(BufferType type);
    BufferType GetBufferType() const;

    void SetLastEditLocation(ByteIndex loc);
    ByteIndex GetLastEditLocation() const;

    ZepMode* GetMode() const;
    void SetMode(std::shared_ptr<ZepMode> spMode);

    using tLineWidgets = std::vector<std::shared_ptr<ILineWidget>>;
    void AddLineWidget(long line, std::shared_ptr<ILineWidget> spWidget);
    void ClearLineWidgets(long line);
    const tLineWidgets* GetLineWidgets(long line) const;

    uint64_t GetLastUpdateTime() const
    {
        return m_lastUpdateTime;
    }

    uint64_t GetUpdateCount() const
    {
        return m_updateCount;
    }

    bool IsHidden() const;

private:
    void ClearRangeMarker(std::shared_ptr<RangeMarker> spMarker);

    void MarkUpdate();

    void UpdateForInsert(const ByteIndex& startOffset, const ByteIndex& endOffset);
    void UpdateForDelete(const ByteIndex& startOffset, const ByteIndex& endOffset);

private:
    // Buffer & record of the line end locations
    GapBuffer<uint8_t> m_gapBuffer;
    std::vector<ByteIndex> m_lineEnds;

    // File and modification info
    ZepPath m_filePath;
    std::string m_strName;
    uint32_t m_fileFlags = 0;
    BufferType m_bufferType = BufferType::Normal;
    ByteIndex m_lastEditLocation = 0;
    uint64_t m_updateCount = 0;
    uint64_t m_lastUpdateTime = 0;

    // Syntax and theme
    std::shared_ptr<ZepSyntax> m_spSyntax;
    std::shared_ptr<ZepTheme> m_spOverrideTheme;
    SyntaxProvider m_syntaxProvider;

    // Widgets
    std::map<ByteIndex, std::vector<std::shared_ptr<ILineWidget>>> m_lineWidgets;

    // Selections
    BufferByteRange m_selection;
    tRangeMarkers m_rangeMarkers;

    // Modes
    std::shared_ptr<ZepMode> m_spMode;
};

// Notification payload
enum class BufferMessageType
{
    // Inform clients that we are about to mess with the buffer
    PreBufferChange = 0,
    TextChanged,
    TextDeleted,
    TextAdded,
    Loaded,
    MarkersChanged
};

struct BufferMessage : public ZepMessage
{
    BufferMessage(ZepBuffer* pBuff, BufferMessageType messageType, const ByteIndex& startLoc, const ByteIndex& endLoc)
        : ZepMessage(Msg::Buffer)
        , pBuffer(pBuff)
        , type(messageType)
        , startLocation(startLoc)
        , endLocation(endLoc)
    {
    }

    ZepBuffer* pBuffer;
    BufferMessageType type;
    ByteIndex startLocation;
    ByteIndex endLocation;
};

class GlyphIterator
{
public:
    using itrGlyph = GapBuffer<uint8_t>::const_iterator;

    GlyphIterator(const ZepBuffer& buffer, ByteIndex offset = 0)
        : m_buffer(buffer),
        m_itr(buffer.GetText().begin() + offset)
    {
        assert(Valid());
    }

    itrGlyph Itr() const { return m_itr; }

    bool Valid() const
    {
        return true;
        /*
        // End iterator is OK
        if (m_itr == m_buffer.GetText().end())
        {
            return true;
        }
        return (!utf8_is_trailing(Char()));
        */
    }

    itrGlyph Begin() const
    {
        return m_buffer.GetText().begin();
    }

    itrGlyph End() const
    {
        return m_buffer.GetText().end();
    }

    bool operator<(const GlyphIterator& rhs) const
    {
        return m_itr < rhs.Itr();
    }

    bool operator<=(const GlyphIterator& rhs) const
    {
        return m_itr <= rhs.Itr();
    }
    bool operator>(const GlyphIterator& rhs) const
    {
        return m_itr > rhs.Itr();
    }

    bool operator>=(const GlyphIterator& rhs) const
    {
        return m_itr >= rhs.Itr();
    }

    bool operator==(const GlyphIterator& rhs) const
    {
        return m_itr == rhs.Itr();
    }

    bool operator!=(const GlyphIterator& rhs) const
    {
        return m_itr != rhs.Itr();
    }

    ByteIndex ToByteIndex() const
    {
        return ByteIndex(m_itr - Begin());
    }

    operator ByteIndex()
    {
        return ByteIndex(m_itr - Begin());
    }

    char Char() const
    {
        return (char)*m_itr;
    }

    GlyphIterator& MoveClamped(long count, LineLocation clamp = LineLocation::LineLastNonCR)
    {
        if (count >= 0)
        {
            auto lineEnd = GlyphIterator(m_buffer, m_buffer.GetLinePos(ToByteIndex(), clamp));
            for (long c = 0; c < count; c++)
            {
                if (m_itr >= lineEnd.Itr())
                {
                    break;
                }
                m_itr += utf8_codepoint_length(*m_itr);
            }
        }
        else
        {
            auto lineBegin = GlyphIterator(m_buffer, m_buffer.GetLinePos(ToByteIndex(), LineLocation::LineBegin));
            for (long c = count; c < 0; c++)
            {
                while ((m_itr > lineBegin.Itr()) && utf8_is_trailing(*(--m_itr)));
            }
        }
        assert(Valid());

        return *this;
    }

    GlyphIterator& Move(long count)
    {
        if (count >= 0)
        {
            for (long c = 0; c < count; c++)
            {
                m_itr += utf8_codepoint_length(*m_itr);
            }
        }
        else
        {
            auto itrBegin = Begin();
            for (long c = count; c < 0; c++)
            {
                while ((m_itr > itrBegin) && utf8::internal::is_trail(*(--m_itr)));
            }
        }
        assert(Valid());
        return *this;
    }

    GlyphIterator Peek(long count)
    {
        GlyphIterator copy(m_buffer, ToByteIndex());
        copy.Move(count);
        return copy;
    }

    GlyphIterator PeekClamped(long count, LineLocation clamp = LineLocation::LineLastNonCR)
    {
        GlyphIterator copy(m_buffer, ToByteIndex());
        copy.MoveClamped(count, clamp);
        return copy;
    }

private:
    const ZepBuffer& m_buffer;
    itrGlyph m_itr;
};

} // namespace Zep
