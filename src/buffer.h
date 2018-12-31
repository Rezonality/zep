#pragma once

#include "editor.h"
#include <mcommon/file/file.h>

#include <set>
#include <shared_mutex>

#include "gap_buffer.h"
#if !(TARGET_PC)
#define shared_mutex shared_timed_mutex
#endif
namespace Zep
{

class ZepSyntax;
class ZepTheme;

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
    TerminatedWithZero = (1 << 1)
};
};

enum class LineLocation
{
    LineFirstGraphChar, // First non blank character
    LineLastGraphChar, // Last non blank character
    LineLastNonCR, // Last character before the carriage return
    LineBegin, // Beginning of line
    BeyondLineEnd, // The line end of the buffer line (for wrapped lines).
    LineCRBegin, // The first carriage return character
};

using BufferLocation = long;
using BufferRange = std::pair<BufferLocation, BufferLocation>;

const long InvalidOffset = -1;

extern const char* Msg_Buffer;

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

    BufferLocation Search(const std::string& str,
        BufferLocation start,
        SearchDirection dir = SearchDirection::Forward,
        BufferLocation end = BufferLocation{ -1l }) const;

    BufferLocation GetLinePos(BufferLocation bufferLocation, LineLocation lineLocation) const;
    bool GetLineOffsets(const long line, long& charStart, long& charEnd) const;
    BufferLocation Clamp(BufferLocation location) const;
    BufferLocation ClampToVisibleLine(BufferLocation in) const;
    long GetBufferColumn(BufferLocation location) const;

    ThreadPool& GetThreadPool()
    {
        return m_threadPool;
    }

    using fnMatch = std::function<bool(const char)>;

    void Move(BufferLocation& loc, SearchDirection dir) const;
    bool Valid(BufferLocation locataion) const;
    bool MotionBegin(BufferLocation& start, uint32_t searchType, SearchDirection dir) const;
    bool Skip(fnMatch IsToken, BufferLocation& start, SearchDirection dir) const;
    bool SkipNot(fnMatch IsToken, BufferLocation& start, SearchDirection dir) const;

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
    BufferLocation LocationFromOffsetByChars(const BufferLocation& location, long offset) const;
    BufferLocation EndLocation() const;

    const GapBuffer<utf8>& GetText() const
    {
        return m_gapBuffer;
    }
    const std::vector<long> GetLineEnds() const
    {
        return m_lineEnds;
    }
    bool IsDirty() const
    {
        return m_dirty;
    }
    bool IsReadOnly() const
    {
        return m_readOnly;
    }
    bool IsViewOnly() const
    {
        return m_viewOnly;
    }
    void SetReadOnly(bool ro)
    {
        m_readOnly = ro;
    }
    void SetViewOnly(bool ro)
    {
        m_viewOnly = ro;
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
private:
    // Internal
    GapBuffer<utf8>::const_iterator SearchWord(uint32_t searchType, GapBuffer<utf8>::const_iterator itrBegin, GapBuffer<utf8>::const_iterator itrEnd, SearchDirection dir) const;

    void ProcessInput(const std::string& str);

private:
    bool m_dirty; // Is the text modified?
    bool m_readOnly = false; // Is the text read only?
    bool m_viewOnly = false; // Is the text not editable, only view?
    GapBuffer<utf8> m_gapBuffer; // Storage for the text - a gap buffer for efficiency
    std::vector<long> m_lineEnds; // End of each line
    ThreadPool m_threadPool;
    uint32_t m_flags;
    std::shared_ptr<ZepSyntax> m_spSyntax;
    std::string m_strName;
    uint32_t m_fileFlags;
    fs::path m_filePath;
    std::shared_ptr<ZepTheme> m_spOverrideTheme;
};

// Notification payload
enum class BufferMessageType
{
    PreBufferChange = 0,
    TextChanged,
    TextDeleted,
    TextAdded,
};
struct BufferMessage : public ZepMessage
{
    BufferMessage(ZepBuffer* pBuff, BufferMessageType messageType, const BufferLocation& startLoc, const BufferLocation& endLoc, const BufferLocation& cursor = BufferLocation{ -1 })
        : ZepMessage(Msg_Buffer)
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
