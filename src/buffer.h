#pragma once

#include "mcommon/file/file.h"
#include "editor.h"
#include "theme.h"

#include <set>
#include "gap_buffer.h"

namespace Zep
{

class ZepSyntax;
class ZepTheme;
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
    Dirty = (1 << 4),  // Has the file been changed?
    NotYetSaved = (1 << 5),
    FirstInit = (1 << 6)
};
};

enum class LineLocation
{
    None,               // Not any specific location
    LineFirstGraphChar, // First non blank character
    LineLastGraphChar,  // Last non blank character
    LineLastNonCR,      // Last character before the carriage return
    LineBegin,          // Beginning of line
    BeyondLineEnd,      // The line end of the buffer line (for wrapped lines).
    LineCRBegin,        // The first carriage return character
};

using BufferLocation = long;
using BufferRange = std::pair<BufferLocation, BufferLocation>;

namespace RangeMarkerDisplayType
{
enum
{
    Underline = (1 << 0),   // Underline the range
    Background = (1 << 1),  // Add a background to the range
    Tooltip = (1 << 2),     // Show a tooltip using the name/description
    Indicator = (1 << 3),    // Show an indicator on the left side
    All = Underline | Tooltip | Indicator
};
};

struct RangeMarker
{
    long bufferLine = -1;
    BufferRange range;
    ThemeColor textColor;
    ThemeColor highlightColor;
    uint32_t displayType = RangeMarkerDisplayType::All;
    std::string name;
    std::string description;
};

using tRangeMarkers = std::vector<std::shared_ptr<RangeMarker>>;

const long InvalidOffset = -1;

// A really big cursor move; which will likely clamp
static const long MaxCursorMove = long(0xFFFFFFF);

class ZepBuffer : public ZepComponent
{
public:
    ZepBuffer(ZepEditor& editor, const std::string& strName);
    virtual ~ZepBuffer();
    void SetText(const std::string& strText);
    void Load(const fs::path& path);
    bool Save(int64_t& size);

    fs::path GetFilePath() const;
    void SetFilePath(const fs::path& path);

    BufferLocation Search(const std::string& str, BufferLocation start, SearchDirection dir = SearchDirection::Forward, BufferLocation end = BufferLocation{-1l}) const;

    BufferLocation GetLinePos(BufferLocation bufferLocation, LineLocation lineLocation) const;
    bool GetLineOffsets(const long line, long& charStart, long& charEnd) const;
    BufferLocation Clamp(BufferLocation location) const;
    BufferLocation ClampToVisibleLine(BufferLocation in) const;
    long GetBufferColumn(BufferLocation location) const;
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
    void SetSyntax(std::shared_ptr<ZepSyntax> spSyntax)
    {
        m_spSyntax = spSyntax;
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

private:
    // Internal
    GapBuffer<utf8>::const_iterator SearchWord(uint32_t searchType, GapBuffer<utf8>::const_iterator itrBegin, GapBuffer<utf8>::const_iterator itrEnd, SearchDirection dir) const;

    void ProcessInput(const std::string& str);

    void UpdateForInsert(const BufferLocation& startOffset, const BufferLocation& endOffset);
    void UpdateForDelete(const BufferLocation& startOffset, const BufferLocation& endOffset);

private:
    bool m_dirty = false;         // Is the text modified?
    GapBuffer<utf8> m_gapBuffer;  // Storage for the text - a gap buffer for efficiency
    std::vector<long> m_lineEnds; // End of each line
    uint32_t m_fileFlags = FileFlags::NotYetSaved | FileFlags::FirstInit;
    std::shared_ptr<ZepSyntax> m_spSyntax;
    std::string m_strName;
    fs::path m_filePath;
    std::shared_ptr<ZepTheme> m_spOverrideTheme;

    BufferRange m_selection;
    tRangeMarkers m_rangeMarkers;
};

// Notification payload
enum class BufferMessageType
{
    PreBufferChange = 0,
    TextChanged,
    TextDeleted,
    TextAdded,
    Initialized
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
