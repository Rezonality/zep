#include <cctype>
#include <cstdint>
#include <cstdlib>

#include "buffer.h"
#include "utils/stringutils.h"

#include <algorithm>
#include <regex>

namespace
{
// A VIM-like definition of a word.  Actually, in Vim this can be changed, but this editor
// assumes a word is alphanumberic or underscore for consistency
inline bool IsWordChar(const char ch) { return std::isalnum(ch) || ch == '_'; }
inline bool IsNonWordChar(const char ch) { return (!IsWordChar(ch) && !std::isspace(ch)); }
inline bool IsWORDChar(const char ch) { return std::isgraph(ch); }
inline bool IsNonWORDChar(const char ch) { return !IsWORDChar(ch) && !std::isspace(ch); }
inline bool IsSpace(const char ch) { return std::isspace(ch); }
}

namespace Zep
{

const char* Msg_Buffer = "Buffer";
ZepBuffer::ZepBuffer(ZepEditor& editor, const std::string& strName)
    : ZepComponent(editor),
    m_threadPool(),
    m_strName(strName)
{
    SetText("");
}

ZepBuffer::~ZepBuffer()
{
}

void ZepBuffer::Notify(std::shared_ptr<ZepMessage> message)
{

}

BufferLocation ZepBuffer::LocationFromOffsetByChars(const BufferLocation& location, long offset) const
{
    // Walk and find.
    long dir = offset > 0 ? 1 : -1;

    // TODO: This can be cleaner(?)
    long current = location;
    for (long i = 0; i < std::abs(offset); i++)
    {
        // If walking back, move back before looking at char
        if (dir == -1)
            current += dir;

        if (current >= m_gapBuffer.size())
            break;

        if (m_gapBuffer[current] == '\n')
        {
            if ((current + dir) >= m_gapBuffer.size())
            {
                break;
            }
        }

        // If walking forward, post append
        if (dir == 1)
        {
            current += dir;
        }
    }

    return LocationFromOffset(current);
}

BufferLocation ZepBuffer::LocationFromOffset(const BufferLocation& location, long offset) const
{
    return LocationFromOffset(location + offset);
}

long ZepBuffer::LineFromOffset(long offset) const
{
    auto itrLine = std::lower_bound(m_lineEnds.begin(), m_lineEnds.end(), offset);
    if (itrLine != m_lineEnds.end() && offset >= *itrLine)
    {
        itrLine++;
    }
    long line = long(itrLine - m_lineEnds.begin());
    line = std::min(std::max(0l, line), long(m_lineEnds.size() - 1));
    return line;
}

BufferLocation ZepBuffer::LocationFromOffset(long offset) const
{
    return BufferLocation{ offset };
}

BufferLocation ZepBuffer::Search(const std::string& str, BufferLocation start, SearchDirection dir, BufferLocation end) const
{
    return BufferLocation{ 0 };
}

// Given a stream of ___AAA__BBB
// We return markers for the start of the first block, beyond the first block, and the second
// i.e. we 'find' AAA and BBB, and remember if there are spaces between them
// This enables us to implement various block motions
BufferBlock ZepBuffer::GetBlock(uint32_t searchType, BufferLocation start, SearchDirection dir) const
{
    BufferBlock ret;
    ret.blockSearchPos = start;

    BufferLocation end;
    BufferLocation begin;
    end = BufferLocation{ long(m_gapBuffer.size()) };
    begin = BufferLocation{ 0 };

    auto itrBegin = m_gapBuffer.begin() + begin;
    auto itrEnd = m_gapBuffer.begin() + end;
    auto itrCurrent = m_gapBuffer.begin() + start;

    auto pIsBlock = IsWORDChar;
    auto pIsNotBlock = IsNonWORDChar;
    if (searchType & SearchType::AlphaNumeric)
    {
        pIsBlock = IsWordChar;
        pIsNotBlock = IsNonWordChar;
    }

    // Set the search pos
    ret.blockSearchPos = LocationFromOffset(long(itrCurrent - m_gapBuffer.begin()));

    auto inc = (dir == SearchDirection::Forward) ? 1 : -1;
    if (inc == -1)
    {
        std::swap(itrBegin, itrEnd);
    }
    ret.direction = inc;

    ret.spaceBefore = false;
    ret.spaceBetween = false;

    while ((itrCurrent != itrBegin) &&
        IsSpace(*itrCurrent))
    {
        itrCurrent -= inc;
    }
    if (itrCurrent != itrBegin)
    {
        itrCurrent += inc;
    }
    ret.spaceBeforeStart = LocationFromOffset(long(itrCurrent - m_gapBuffer.begin()));

    // Skip the initial spaces; they are not part of the block
    itrCurrent = m_gapBuffer.begin() + start;
    while (itrCurrent != itrEnd &&
        (IsSpace(*itrCurrent)))
    {
        ret.spaceBefore = true;
        itrCurrent += inc;
    }

    auto GetBlockChecker = [&](const char ch) 
    {
        if (pIsBlock(ch))
            return pIsBlock;
        else 
            return pIsNotBlock;
    };

    // Find the right start block type
    auto pCheck = GetBlockChecker(*itrCurrent);
    ret.startOnBlock = (pCheck == pIsBlock) ? true : false;

    // Walk backwards to the start of the block
    while (itrCurrent != itrBegin &&
        (pCheck(*itrCurrent)))
    {
        itrCurrent -= inc;
    }
    if (itrCurrent < m_gapBuffer.end() &&
        !pCheck(*itrCurrent))  // Note this also handles where we couldn't walk back any further
    {
        itrCurrent += inc;
    }

    // Record start
    ret.firstBlock = LocationFromOffset(long(itrCurrent - m_gapBuffer.begin()));
   
    // Walk forwards to the end of the block
    while (itrCurrent != itrEnd &&
        (pCheck(*itrCurrent)))
    {
        itrCurrent += inc;
    }

    // Record end
    ret.firstNonBlock = LocationFromOffset(long(itrCurrent - m_gapBuffer.begin()));

    // If we couldn't walk further back, record that the offset was beyond!
    // This is only for backward motions
    if (itrCurrent < m_gapBuffer.end() &&
        pCheck(*itrCurrent))
    {
        ret.firstNonBlock += inc;
    }

    // Skip the next spaces; they are not part of the block
    while (itrCurrent != itrEnd &&
        (IsSpace(*itrCurrent)))
    {
        ret.spaceBetween = true;
        itrCurrent += inc;
    }

    ret.secondBlock = LocationFromOffset(long(itrCurrent - m_gapBuffer.begin()));

    // Get to the end of the second non block
    pCheck = GetBlockChecker(*itrCurrent);
    while (itrCurrent != itrEnd &&
        (pCheck(*itrCurrent)))
    {
        itrCurrent += inc;
    }

    ret.secondNonBlock = LocationFromOffset(long(itrCurrent - m_gapBuffer.begin()));
    
    // If we couldn't walk further back, record that the offset was beyond!
    if (itrCurrent < m_gapBuffer.end() &&
        pCheck(*itrCurrent))
    {
        ret.secondNonBlock += inc;
    }
    
    return ret;
}

void ZepBuffer::ProcessInput(const std::string& text)
{
    // Inform clients we are about to change the buffer
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, 0, BufferLocation(m_gapBuffer.size() - 1)));

    m_gapBuffer.clear();
    m_lineEnds.clear();

    m_bStrippedCR = false;

    if (text.empty())
    {
        m_gapBuffer.push_back(0);
    }
    else
    {
        // Update the gap buffer with the text
        // We remove \r, we only care about \n
        for (auto& ch : text)
        {
            if (ch == '\r')
            {
                m_bStrippedCR = true;
            }
            else
            {
                m_gapBuffer.push_back(ch);
                if (ch == '\n')
                {
                    m_lineEnds.push_back(long(m_gapBuffer.size()));
                }
            }
        }
    }

    if (m_gapBuffer[m_gapBuffer.size() - 1] != 0)
    {
        m_gapBuffer.push_back(0);
    }

    m_lineEnds.push_back(long(m_gapBuffer.size()));
}

BufferLocation ZepBuffer::Clamp(BufferLocation in) const
{
    in = std::min(in, BufferLocation(m_gapBuffer.size() - 1));
    in = std::max(in, BufferLocation(0));
    return in;
}

// Method for querying the beginning and end of a line
bool ZepBuffer::GetLineOffsets(const long line, long& lineStart, long& lineEnd) const
{
    // Not valid
    if (m_lineEnds.size() <= line)
    {
        lineStart = 0;
        lineEnd = 0;
        return false;
    }

    // Find the line bounds - we know the end, find the start from the previous
    lineEnd = m_lineEnds[line];
    lineStart = line == 0 ? 0 : m_lineEnds[line - 1];
    return true;
}

/* TODO:
void ZepBuffer::LockRead()
{
    // Wait for line counter
    if (m_lineCountResult.valid())
    {
        m_lineCountResult.get();
    }
}*/


// Replace the buffer buffer with the text 
void ZepBuffer::SetText(const std::string& text)
{
    if (m_gapBuffer.size() != 0)
    {
        GetEditor().Broadcast(std::make_shared<BufferMessage>(this,
            BufferMessageType::TextDeleted,
            BufferLocation{ 0 },
            BufferLocation{ long(m_gapBuffer.size()) }));
    }

    ProcessInput(text);

    GetEditor().Broadcast(std::make_shared<BufferMessage>(this,
        BufferMessageType::TextAdded,
        BufferLocation{ 0 },
        BufferLocation{ long(m_gapBuffer.size()) }));

    // Doc is not dirty
    m_dirty = 0;
}


BufferLocation ZepBuffer::GetLinePos(long line, LineLocation location) const
{
    // Clamp the line
    if (m_lineEnds.size() <= line)
    {
        line = long(m_lineEnds.size()) - 1l;
        line = std::max(0l, line);
    }
    else if (line < 0)
    {
        line = 0l;
    }

    BufferLocation ret{ 0 };
    if (m_lineEnds.empty())
    {
        ret = 0;
        return ret;
    }

    auto searchEnd = m_lineEnds[line];
    auto searchStart = 0;
    if (line > 0)
    {
        searchStart = m_lineEnds[line - 1];
    }

    switch (location)
    {
    default:
    case LineLocation::LineBegin:
        ret = searchStart;
        break;

    case LineLocation::LineEnd:
        ret = searchEnd;
        break;

    case LineLocation::LineCRBegin:
    {
        auto loc = std::find_if(m_gapBuffer.begin() + searchStart, m_gapBuffer.end() + searchEnd,
            [&](const utf8& ch)
        {
            if (ch == '\n' || ch == 0)
                return true;
            return false;
        });
        ret = long(loc - m_gapBuffer.begin());
    }
    break;

    case LineLocation::LineFirstGraphChar:
    {
        auto loc = std::find_if(m_gapBuffer.begin() + searchStart, m_gapBuffer.end() + searchEnd,
            [&](const utf8& ch) { return ch != 0 && std::isgraph(ch); });
        ret = long(loc - m_gapBuffer.begin());
    }
    break;

    case LineLocation::LineLastNonCR:
    {
        auto begin = std::reverse_iterator<GapBuffer<utf8>::const_iterator>(m_gapBuffer.begin() + searchEnd);
        auto end = std::reverse_iterator<GapBuffer<utf8>::const_iterator>(m_gapBuffer.begin() + searchStart);
        auto loc = std::find_if(begin, end,
            [&](const utf8& ch)
        {
            return ch != '\n' && ch != 0;
        });
        if (loc == end)
        {
            ret = searchEnd;
        }
        else
        {
            ret = long(loc.base() - m_gapBuffer.begin() - 1);
        }
    }
    break;

    case LineLocation::LineLastGraphChar:
    {
        auto begin = std::reverse_iterator<GapBuffer<utf8>::const_iterator>(m_gapBuffer.begin() + searchEnd);
        auto end = std::reverse_iterator<GapBuffer<utf8>::const_iterator>(m_gapBuffer.begin() + searchStart);
        auto loc = std::find_if(begin, end,
            [&](const utf8& ch) { return std::isgraph(ch); });
        if (loc == end)
        {
            ret = searchEnd;
        }
        else
        {
            ret = long(loc.base() - m_gapBuffer.begin() - 1);
        }
    }
    break;
    }

    return ret;
}

bool ZepBuffer::Insert(const BufferLocation& startOffset, const std::string& str, const BufferLocation& cursorAfter)
{
    if (startOffset > m_gapBuffer.size())
    {
        return false;
    }

    BufferLocation changeRange{ long(m_gapBuffer.size()) };

    // We are about to modify this range
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startOffset, changeRange));

    // abcdef\r\nabc<insert>dfdf\r\n
    auto itrLine = std::lower_bound(m_lineEnds.begin(), m_lineEnds.end(), startOffset);;
    if (itrLine != m_lineEnds.end() &&
        *itrLine <= startOffset)
    {
        itrLine++;
    }

    auto itrEnd = str.end();
    auto itrBegin = str.begin();
    auto itr = str.begin();

    std::vector<long> lines;
    std::string lineEndSymbols("\n");
    while (itr != itrEnd)
    {
        // Get to first point after "\n" 
        // That's the point just after the end of the current line
        itr = std::find_first_of(itr, itrEnd, lineEndSymbols.begin(), lineEndSymbols.end());
        if (itr != itrEnd)
        {
            if (itr != itrEnd &&
                *itr == '\n')
            {
                itr++;
            }
            lines.push_back(long(itr - itrBegin) + startOffset);
        }
    }

    // Increment the rest of the line ends
    // We make all the remaning line ends bigger by the size of the insertion
    auto itrAdd = itrLine;
    while (itrAdd != m_lineEnds.end())
    {
        *itrAdd += long(str.length());
        itrAdd++;
    }

    if (!lines.empty())
    {
        // Update the atomic line counter so clients can see where we are up to.
        m_lineEnds.insert(itrLine, lines.begin(), lines.end());
    }

    m_gapBuffer.insert(m_gapBuffer.begin() + startOffset, str.begin(), str.end());

    // This is the range we added (not valid any more in the buffer)
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextAdded, startOffset, changeRange, cursorAfter));
    return true;
}

// A fundamental operation - delete a range of characters
// Need to update:
// - m_lineEnds
// - m_processedLine
// - m_buffer (i.e remove chars)
// We also need to inform clients before we change the buffer, and after we delete text with the range we removed.
// This helps them to fix up their data structures without rebuilding.
// Assumption: The buffer always is at least a single line/character of '0', representing file end.
// This makes a few things fall out more easily
bool ZepBuffer::Delete(const BufferLocation& startOffset, const BufferLocation& endOffset, const BufferLocation& cursorAfter)
{
    assert(startOffset >= 0 && endOffset <= (m_gapBuffer.size() - 1));

    // We are about to modify this range
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startOffset, endOffset));

    auto itrLine = std::lower_bound(m_lineEnds.begin(), m_lineEnds.end(), startOffset);
    if (itrLine == m_lineEnds.end())
    {
        return false;
    }

    auto itrLastLine = std::upper_bound(itrLine, m_lineEnds.end(), endOffset);
    auto offsetDiff = endOffset - startOffset;

    if (*itrLine <= startOffset)
    {
        itrLine++;
    }

    // Adjust all line offsets beyond us
    for (auto itr = itrLastLine; itr != m_lineEnds.end(); itr++)
    {
        *itr -= offsetDiff;
    }

    if (itrLine != itrLastLine)
    {
        m_lineEnds.erase(itrLine, itrLastLine);
    }

    m_gapBuffer.erase(m_gapBuffer.begin() + startOffset, m_gapBuffer.begin() + endOffset);
    assert(m_gapBuffer.size() > 0 && m_gapBuffer[m_gapBuffer.size() - 1] == 0);

    // This is the range we deleted (not valid any more in the buffer)
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextDeleted, startOffset, endOffset, cursorAfter));

    return true;
}

BufferLocation ZepBuffer::EndLocation() const
{
    auto end = m_gapBuffer.size() - 1;
    return LocationFromOffset(long(end));
}

} // Zep namespace
