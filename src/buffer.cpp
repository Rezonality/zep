#include <cctype>
#include <cstdint>
#include <cstdlib>

#include "buffer.h"
#include "utils/stringutils.h"
#include "utils/file.h"

#include <algorithm>
#include <regex>

namespace
{
// A VIM-like definition of a word.  Actually, in Vim this can be changed, but this editor
// assumes a word is alphanumberic or underscore for consistency
inline bool IsWordChar(const char ch)
{
    return std::isalnum(ch) || ch == '_';
}
inline bool IsWordOrSepChar(const char ch)
{
    return std::isalnum(ch) || ch == '_' || ch == ' ' || ch == '\n' || ch == 0;
}
inline bool IsWORDChar(const char ch)
{
    return std::isgraph(ch);
}
inline bool IsWORDOrSepChar(const char ch)
{
    return std::isgraph(ch) || ch == ' ' || ch == '\n' || ch == 0;
}
inline bool IsSpace(const char ch)
{
    return ch == ' ';
}
using fnMatch = std::function<bool>(const char);

} // namespace

namespace Zep
{

const char* Msg_Buffer = "Buffer";
ZepBuffer::ZepBuffer(ZepEditor& editor, const std::string& strName)
    : ZepComponent(editor)
    , m_threadPool()
    , m_strName(strName)
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

bool ZepBuffer::Valid(BufferLocation location) const
{
    if (location < 0 || location >= m_gapBuffer.size())
    {
        return false;
    }
    return true;
}

// Prepare for a motion
bool ZepBuffer::MotionBegin(BufferLocation& start, uint32_t searchType, SearchDirection dir) const
{
    BufferLocation newStart = start;

    // Clamp to sensible, begin
    newStart = std::min(newStart, BufferLocation(m_gapBuffer.size() - 1));
    newStart = std::max(0l, newStart);

    bool change = newStart != start;
    if (change)
    {
        start = newStart;
        return true;
    }
    return start;
}

void ZepBuffer::Move(BufferLocation& loc, SearchDirection dir) const
{
    if (dir == SearchDirection::Backward)
        loc--;
    else
        loc++;
}

bool ZepBuffer::Skip(fnMatch IsToken, BufferLocation& start, SearchDirection dir) const
{
    if (!Valid(start))
        return false;

    bool moved = false;
    while (Valid(start) && IsToken(m_gapBuffer[start]))
    {
        Move(start, dir);
        moved = true;
    }
    return moved;
}

bool ZepBuffer::SkipNot(fnMatch IsToken, BufferLocation& start, SearchDirection dir) const
{
    if (!Valid(start))
        return false;

    bool moved = false;
    while (Valid(start) && !IsToken(m_gapBuffer[start]))
    {
        Move(start, dir);
        moved = true;
    }
    return moved;
}

// This is the vim-like 'caw' rule; The motions and behaviour are based on how vim behaves.
// This is still quite complex behavior for this particular motion.  I'm open to better ways to express it!
BufferRange ZepBuffer::AWordMotion(BufferLocation start, uint32_t searchType) const
{
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    BufferRange r;
    r.first = start;

    MotionBegin(start, searchType, SearchDirection::Forward);

    // Already on a word; find the limits, and include the space
    if (Skip(IsWord, start, SearchDirection::Backward))
    {
        start += 1;
        r.first = start;
        Skip(IsWord, start, SearchDirection::Forward);
        Skip(IsSpace, start, SearchDirection::Forward);
        r.second = start;
    }
    // ... or skip space
    else if (Skip(IsSpace, start, SearchDirection::Forward))
    {
        Skip(IsWord, start, SearchDirection::Forward);
        r.second = start;
    }
    // On a non-word, find the beginning, remove including following spaces
    else if (SkipNot(IsWord, start, SearchDirection::Backward))
    {
        Skip(IsSpace, start, SearchDirection::Forward);
        start += 1;
        r.first = start;
        SkipNot(IsWord, start, SearchDirection::Forward);
        Skip(IsSpace, start, SearchDirection::Forward);
        r.second = start;
    }

    return r;
}

BufferRange ZepBuffer::InnerWordMotion(BufferLocation start, uint32_t searchType) const
{
    auto IsWordOrSpace = searchType == SearchType::Word ? IsWordOrSepChar : IsWORDOrSepChar;
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;
    MotionBegin(start, searchType, SearchDirection::Forward);

    BufferRange r;

    if (SkipNot(IsWordOrSpace, start, SearchDirection::Forward))
    {
        r.second = start;
        start--;
        SkipNot(IsWordOrSpace, start, SearchDirection::Backward);
        r.first = start + 1;
    }
    else if (Skip(IsSpace, start, SearchDirection::Forward))
    {
        r.second = start;
        start--;
        Skip(IsSpace, start, SearchDirection::Backward);
        r.first = start + 1;
    }
    else
    {
        Skip(IsWord, start, SearchDirection::Forward);
        r.second = start;
        start--;
        Skip(IsWord, start, SearchDirection::Backward);
        r.first = start + 1;
    }
    return r;
}

BufferLocation ZepBuffer::WordMotion(BufferLocation start, uint32_t searchType, SearchDirection dir) const
{
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    MotionBegin(start, searchType, dir);

    if (dir == SearchDirection::Forward)
    {
        if (Skip(IsWord, start, dir))
        {
            // Skipped a word, skip spaces then done
            Skip(IsSpace, start, dir);
        }
        else
        {
            SkipNot(IsWord, start, dir);
        }
    }
    else // Backward
    {
        auto startSearch = start;

        // Jump back to the beginning of a word if on it
        if (Skip(IsWord, start, dir))
        {
            // If we weren't already on the first char of the word, then we have gone back a word!
            if (startSearch != (start + 1))
            {
                SkipNot(IsWord, start, SearchDirection::Forward);
                return start;
            }
        }
        else
        {
            SkipNot(IsWord, start, dir);
        }

        // Skip any spaces
        Skip(IsSpace, start, dir);

        // Go back to the beginning of the word
        if (Skip(IsWord, start, dir))
        {
            SkipNot(IsWord, start, SearchDirection::Forward);
        }
    }
    return start;
}

BufferLocation ZepBuffer::EndWordMotion(BufferLocation start, uint32_t searchType, SearchDirection dir) const
{
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    MotionBegin(start, searchType, dir);

    if (dir == SearchDirection::Forward)
    {
        auto startSearch = start;

        // Skip to the end
        if (Skip(IsWord, start, dir))
        {
            // We moved a bit, so we found the end of the current word
            if (startSearch != start - 1)
            {
                SkipNot(IsWord, start, SearchDirection::Backward);
                return start;
            }
        }
        else
        {
            SkipNot(IsWord, start, dir);
        }

        // Skip any spaces
        Skip(IsSpace, start, dir);

        // Go back to the beginning of the word
        if (Skip(IsWord, start, dir))
        {
            SkipNot(IsWord, start, SearchDirection::Backward);
        }
    }
    else // Backward
    {
        // Note this is the same as the Next word code, in 'forward' mode
        if (Skip(IsWord, start, dir))
        {
            // Skipped a word, skip spaces then done
            Skip(IsSpace, start, dir);
        }
        else
        {
            SkipNot(IsWord, start, dir);
        }
    }
    return start;
}

BufferLocation ZepBuffer::ChangeWordMotion(BufferLocation start, uint32_t searchType, SearchDirection dir) const
{
    // Change word is different to work skipping; it will change a string of spaces, for example.
    // Essentially it changes 'what you are over', based on the word rule
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;
    MotionBegin(start, searchType, dir);
    if (Skip(IsWord, start, dir))
    {
        return start;
    }
    SkipNot(IsWord, start, dir);
    return start;
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
            else if (ch == '\t')
            {
                m_gapBuffer.push_back(' ');
                m_gapBuffer.push_back(' ');
                m_gapBuffer.push_back(' ');
                m_gapBuffer.push_back(' ');
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

// Basic load suppot; read a file if it's present, but keep
// the file path in case you want to write later
void ZepBuffer::Load(const fs::path& path)
{
    m_filePath = path;
    
    auto read = file_read(path);
    if (!read.empty())
    {
        SetText(read);
    }
}

bool ZepBuffer::Save()
{
    auto str = GetText().string();

    // Put back /r/n if necessary while writing the file
    // At the moment, Zep removes /r/n and just uses /n while modifying text.
    // It replaces the /r on files that had it afterwards
    if (m_bStrippedCR)
    {
        // TODO: faster way to replace newlilnes
        StringUtils::ReplaceStringInPlace(str, "\n", "\r\n");
    }

    // Write
    return Zep::file_write(m_filePath, &str[0], str.size());
}

fs::path ZepBuffer::GetFilePath() const
{
    return m_filePath;
}

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

// TODO: These can be cleaner
BufferLocation ZepBuffer::GetLinePos(BufferLocation bufferLocation, LineLocation lineLocation) const
{
    bufferLocation = Clamp(bufferLocation);
    if (m_gapBuffer.empty())
        return bufferLocation;

    // If we are on the CR, move back 1
    if (m_gapBuffer[bufferLocation] == '\n')
    {
        bufferLocation--;
    }

    // Find the end of the previous line
    while (bufferLocation >= 0 &&
        m_gapBuffer[bufferLocation] != '\n')
    {
        bufferLocation--;
    }

    // Step back to the start of the line
    bufferLocation++;

    switch (lineLocation)
    {
    default:
    case LineLocation::LineBegin:
    {
        return Clamp(bufferLocation);
    }
    break;

    // The point just after the line end
    case LineLocation::BeyondLineEnd:
    {
        while (bufferLocation < m_gapBuffer.size() &&
            m_gapBuffer[bufferLocation] != '\n' &&
            m_gapBuffer[bufferLocation] != 0)
        {
            bufferLocation++;
        }
        bufferLocation++;
        return Clamp(bufferLocation);
    }
    break;

    case LineLocation::LineCRBegin:
    {
        while (bufferLocation < m_gapBuffer.size()
            && m_gapBuffer[bufferLocation] != '\n'
            && m_gapBuffer[bufferLocation] != 0)
        {
            bufferLocation++;
        }
        return bufferLocation;
    }
    break;

    case LineLocation::LineFirstGraphChar:
    {
        while (bufferLocation < m_gapBuffer.size() &&
            !std::isgraph(m_gapBuffer[bufferLocation]) &&
            m_gapBuffer[bufferLocation] != '\n')
        {
            bufferLocation++;
        }
        return Clamp(bufferLocation);
    }
    break;

    case LineLocation::LineLastNonCR:
    {
        auto start = bufferLocation;

        while (bufferLocation < m_gapBuffer.size()
            && m_gapBuffer[bufferLocation] != '\n'
            && m_gapBuffer[bufferLocation] != 0)
        {
            bufferLocation++;
        }

        if (start != bufferLocation)
        {
            bufferLocation--;
        }

        return Clamp(bufferLocation);
    }
    break;

    case LineLocation::LineLastGraphChar:
    {
        while (bufferLocation < m_gapBuffer.size()
            && m_gapBuffer[bufferLocation] != '\n'
            && m_gapBuffer[bufferLocation] != 0)
        {
            bufferLocation++;
        }

        while (bufferLocation > 0 && 
            bufferLocation < m_gapBuffer.size() &&
            !std::isgraph(m_gapBuffer[bufferLocation]))
        {
            bufferLocation--;
        }
        return Clamp(bufferLocation);
    }
    break;
    }

    return bufferLocation;
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
    auto itrLine = std::lower_bound(m_lineEnds.begin(), m_lineEnds.end(), startOffset);
    ;
    if (itrLine != m_lineEnds.end() && *itrLine <= startOffset)
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
            if (itr != itrEnd && *itr == '\n')
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
// - m_pBuffer (i.e remove chars)
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

} // namespace Zep
