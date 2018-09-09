#pragma once

#include "editor.h"

#include <shared_mutex>
#include <set>

#include "gap_buffer.h"
#if !(TARGET_PC)
#define shared_mutex shared_timed_mutex
#endif
namespace Zep
{

class ZepSyntax;

enum class SearchDirection
{
    Forward,
    Backward
};

namespace SearchType
{
enum : uint32_t
{
    Word = (1 << 0),
    Begin = (1 << 1),
    End = (1 << 2),
    AlphaNumeric = (1 << 3)
};
};

enum class LineLocation
{
    LineFirstGraphChar, // First non blank character
    LineLastGraphChar,  // Last non blank character
    LineLastNonCR,      // Last character before the carriage return
    LineBegin,          // Beginning of line
    LineEnd,            // The line end of the buffer line (for wrapped lines).
    LineCRBegin,        // The first carriage return character
};

using BufferLocation = long;
const long InvalidOffset = -1;

extern const char* Msg_Buffer;

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
        : ZepMessage(Msg_Buffer),
        pBuffer(pBuff),
        type(messageType),
        startLocation(startLoc),
        endLocation(endLoc),
        cursorAfter(cursor)
    {
    }

    ZepBuffer* pBuffer;
    BufferMessageType type;
    BufferLocation startLocation;
    BufferLocation endLocation;
    BufferLocation cursorAfter;
};

struct BufferBlock
{
    // White space before the block
    bool spaceBefore;
    bool spaceBetween;
    bool startOnBlock;
    int direction;
    BufferLocation spaceBeforeStart;
    BufferLocation firstBlock;
    BufferLocation firstNonBlock;
    BufferLocation secondBlock;
    BufferLocation secondNonBlock;

    // Start search location
    BufferLocation blockSearchPos;

};


class ZepBuffer : public ZepComponent
{
public:
    ZepBuffer(ZepEditor& editor, const std::string& strName);
    virtual ~ZepBuffer();
    void SetText(const std::string& strText);

    BufferBlock GetBlock(uint32_t searchType, BufferLocation start, SearchDirection dir) const;

    BufferLocation Search(const std::string& str,
        BufferLocation start,
        SearchDirection dir = SearchDirection::Forward,
        BufferLocation end = BufferLocation{ -1l }) const;

    BufferLocation GetLinePos(long line, LineLocation location) const;
    bool GetLineOffsets(const long line, long& charStart, long& charEnd) const;
    BufferLocation Clamp(BufferLocation location) const;

    ThreadPool& GetThreadPool() { return m_threadPool; }

    bool Delete(const BufferLocation& startOffset, const BufferLocation& endOffset, const BufferLocation& cursorAfter = BufferLocation{ -1 });
    bool Insert(const BufferLocation& startOffset, const std::string& str, const BufferLocation& cursorAfter = BufferLocation{ -1 });

    long GetLineCount() const { return long(m_lineEnds.size()); }
    long LineFromOffset(long offset) const;
    BufferLocation LocationFromOffset(const BufferLocation& location, long offset) const;
    BufferLocation LocationFromOffset(long offset) const;
    BufferLocation LocationFromOffsetByChars(const BufferLocation& location, long offset) const;
    BufferLocation EndLocation() const;

    const GapBuffer<utf8>& GetText() const { return m_gapBuffer; }
    const std::vector<long> GetLineEnds() const { return m_lineEnds; }
    bool IsDirty() const { return m_dirty; }
    bool IsReadOnly() const { return m_readOnly; }
    bool IsViewOnly() const { return m_viewOnly; }
    void SetReadOnly(bool ro) { m_readOnly = ro; }
    void SetViewOnly(bool ro) { m_viewOnly = ro; }

    void SetSyntax(std::shared_ptr<ZepSyntax> spSyntax) { m_spSyntax = spSyntax; }
    ZepSyntax* GetSyntax() const { return m_spSyntax.get(); }

    const std::string& GetName() const { return m_strName; }

    virtual void Notify(std::shared_ptr<ZepMessage> message) override;

private:
    // Internal
    GapBuffer<utf8>::const_iterator SearchWord(uint32_t searchType, GapBuffer<utf8>::const_iterator itrBegin, GapBuffer<utf8>::const_iterator itrEnd, SearchDirection dir) const;

    void ProcessInput(const std::string& str);

private:
    bool m_dirty;                              // Is the text modified?
    bool m_readOnly = false;                   // Is the text read only?
    bool m_viewOnly = false;                   // Is the text not editable, only view?
    GapBuffer<utf8> m_gapBuffer;                  // Storage for the text - a gap buffer for efficiency
    std::vector<long> m_lineEnds;              // End of each line
    ThreadPool m_threadPool;
    uint32_t m_flags;
    std::shared_ptr<ZepSyntax> m_spSyntax;
    std::string m_strName;
    bool m_bStrippedCR;
};

} // Zep
