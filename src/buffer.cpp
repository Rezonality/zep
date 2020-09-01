#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <regex>

#include "zep/buffer.h"
#include "zep/editor.h"
#include "zep/filesystem.h"

#include "zep/mcommon/file/path.h"
#include "zep/mcommon/string/stringutils.h"

#include "zep/mcommon/logger.h"

namespace Zep
{

namespace
{

// A VIM-like definition of a word.  Actually, in Vim this can be changed, but this editor
// assumes a word is alphanumeric or underscore for consistency
inline bool IsWordChar(const char c)
{
    auto ch = ToASCII(c);
    return std::isalnum(ch) || ch == '_';
}
inline bool IsWordOrSepChar(const char c)
{
    auto ch = ToASCII(c);
    return std::isalnum(ch) || ch == '_' || ch == ' ' || ch == '\n' || ch == 0;
}
inline bool IsWORDChar(const char c)
{
    auto ch = ToASCII(c);
    return std::isgraph(ch);
}
inline bool IsWORDOrSepChar(const char c)
{
    auto ch = ToASCII(c);
    return std::isgraph(ch) || ch == ' ' || ch == '\n' || ch == 0;
}
inline bool IsSpace(const char c)
{
    auto ch = ToASCII(c);
    return ch == ' ';
}
inline bool IsSpaceOrNewline(const char c)
{
    auto ch = ToASCII(c);
    return ch == ' ' || ch == '\n';
}
inline bool IsSpaceOrTerminal(const char c)
{
    auto ch = ToASCII(c);
    return ch == ' ' || ch == 0 || ch == '\n';
}

using fnMatch = std::function<bool>(const char);

} // namespace
ZepBuffer::ZepBuffer(ZepEditor& editor, const std::string& strName)
    : ZepComponent(editor)
    , m_strName(strName)
{
    Clear();
}

ZepBuffer::ZepBuffer(ZepEditor& editor, const ZepPath& path)
    : ZepComponent(editor)
{
    Load(path);
}

ZepBuffer::~ZepBuffer()
{
}

void ZepBuffer::Notify(std::shared_ptr<ZepMessage> message)
{
}

// Vertical column
long ZepBuffer::GetBufferColumn(GlyphIterator location) const
{
    auto lineStart = GetLinePos(location, LineLocation::LineBegin);
    return CodePointDistance(lineStart, location);
}

// Find the location inside the list of line ends
long ZepBuffer::GetBufferLine(GlyphIterator location) const
{
    auto itrLine = std::lower_bound(m_lineEnds.begin(), m_lineEnds.end(), location.Index());
    if (itrLine != m_lineEnds.end() && location.Index() >= *itrLine)
    {
        itrLine++;
    }
    long line = long(itrLine - m_lineEnds.begin());
    line = std::min(std::max(0l, line), long(m_lineEnds.size() - 1));
    return line;
}

// Prepare for a motion
void ZepBuffer::MotionBegin(GlyphIterator& start) const
{
    GlyphIterator newStart = start;

    // Clamp to sensible, begin
    newStart.Clamp();

    bool change = newStart != start;
    if (change)
    {
        start = newStart;
    }
}

bool ZepBuffer::Move(GlyphIterator& loc, SearchDirection dir) const
{
    if (dir == SearchDirection::Backward && loc.Index() == 0)
    {
        return false;
    }
    else if (dir == SearchDirection::Forward && loc.Index() == End().Index())
    {
        return false;
    }

    if (dir == SearchDirection::Backward)
    {
        loc--;
    }
    else
    {
        loc++;
    }
    return true;
}

bool ZepBuffer::Skip(fnMatch IsToken, GlyphIterator& start, SearchDirection dir) const
{
    if (!start.Valid())
    {
        return false;
    }

    bool found = false;
    auto itrEnd = End();
    auto itrBegin = Begin();

    while (IsToken(start.Char()))
    {
        found = true;
        if (!Move(start, dir))
            break;
    }
    return found;
}

bool ZepBuffer::SkipOne(fnMatch IsToken, GlyphIterator& start, SearchDirection dir) const
{
    if (!start.Valid())
    {
        return false;
    }

    bool found = false;
    if (IsToken(start.Char()))
    {
        found = true;
        Move(start, dir);
    }
    return found;
}

bool ZepBuffer::SkipNot(fnMatch IsToken, GlyphIterator& start, SearchDirection dir) const
{
    if (!start.Valid())
        return false;

    bool found = false;
    while (!IsToken(start.Char()))
    {
        found = true;
        if (!Move(start, dir))
            break;
    }
    return found;
}

// This is the vim-like 'caw' rule; The motions and behaviour are based on how vim behaves.
// This is still quite complex behavior for this particular motion.  I'm open to better ways to express it!
GlyphRange ZepBuffer::AWordMotion(GlyphIterator start, uint32_t searchType) const
{
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    GlyphRange r;
    r.first = start;

    MotionBegin(start);

    // Already on a word; find the limits, and include the space
    if (Skip(IsWord, start, SearchDirection::Backward))
    {
        SkipNot(IsWord, start, SearchDirection::Forward);
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
        Skip(IsWord, start, SearchDirection::Forward);
        r.first = start;
        SkipNot(IsWord, start, SearchDirection::Forward);
        Skip(IsSpace, start, SearchDirection::Forward);
        r.second = start;
    }

    return r;
}

// Implements the ctrl + motion of a standard editor.
// This is a little convoluted; perhaps the logic can be simpler!
// Playing around with CTRL+ arrows and shift in an app like notepad will teach you that the rules for how far to jump
// depend on what you are over, and which direction you are going.....
// The unit tests are designed to enforce the behavior here
GlyphRange ZepBuffer::StandardCtrlMotion(GlyphIterator cursor, SearchDirection searchDir) const
{
    MotionBegin(cursor);

    auto lineEnd = GetLinePos(cursor, LineLocation::LineCRBegin);
    auto current = std::min(lineEnd, cursor.Clamp());

    GlyphRange r;
    r.first = current;
    r.second = current;

    if (searchDir == SearchDirection::Forward)
    {
        if (Skip(IsWORDChar, current, searchDir))
        {
            // Skip space
            Skip(IsSpace, current, searchDir);
        }
        else
        {
            SkipNot(IsWORDChar, current, searchDir);
        }
    }
    else
    {
        // Always skip back a char (iterator will clamp)
        current--;

        // Stop on the newline, or continue
        if (current.Char() != '\n')
        {
            SkipNot(IsWORDChar, current, searchDir);
            Skip(IsWORDChar, current, searchDir);
            SkipNot(IsWORDChar, current, SearchDirection::Forward);
        }
    }
    r.second = current.Clamp();

    return r;
}

// Note:
// In general I'm unhappy with these word motion functions.  They are _hard_ ; especially if you
// want them to conform to the quirks of Vim.  I haven't found a cleaner way than this to make them
// work.
GlyphRange ZepBuffer::InnerWordMotion(GlyphIterator start, uint32_t searchType) const
{
    auto IsWordOrSpace = searchType == SearchType::Word ? IsWordOrSepChar : IsWORDOrSepChar;
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;
    MotionBegin(start);

    GlyphRange r;

    // Special case; change inner word on a newline, stay put, don't delete anything
    if (start.Char() == '\n')
    {
        r.first = r.second = start;
    }
    else if (SkipNot(IsWordOrSpace, start, SearchDirection::Forward))
    {
        r.second = start;
        start--;
        SkipNot(IsWordOrSpace, start, SearchDirection::Backward);
        Skip(IsWordOrSpace, start, SearchDirection::Forward);
        r.first = start;
    }
    else if (Skip(IsSpace, start, SearchDirection::Forward))
    {
        r.second = start;
        start--;
        Skip(IsSpace, start, SearchDirection::Backward);
        SkipNot(IsSpace, start, SearchDirection::Forward);
        r.first = start;
    }
    else
    {
        Skip(IsWord, start, SearchDirection::Forward);
        r.second = start;
        start--;
        Skip(IsWord, start, SearchDirection::Backward);
        SkipNot(IsWord, start, SearchDirection::Forward);
        r.first = start;
    }
    return r;
}

GlyphIterator ZepBuffer::Find(GlyphIterator start, const uint8_t* pBeginString, const uint8_t* pEndString) const
{
    // Should be a valid start
    assert(start.Valid());
    assert(pBeginString != nullptr);

    if (!start.Valid() && pBeginString)
    {
        return start;
    }

    // Find the end of the test string
    if (pEndString == nullptr)
    {
        pEndString = pBeginString;
        while (*pEndString != 0)
        {
            pEndString++;
        }
    }

    auto itrBuffer = start;
    auto itrEnd = End();
    while (itrBuffer != itrEnd)
    {
        auto itrNext = itrBuffer;

        // Loop the string and match it to the buffer
        auto pCurrent = pBeginString;
        while (pCurrent != pEndString && itrNext != itrEnd)
        {
            if (*pCurrent != itrNext.Char())
            {
                break;
            }
            pCurrent++;
            itrNext++;
        };

        // We successfully got to the end
        if (pCurrent == pEndString)
        {
            return itrBuffer;
        }

        itrBuffer++;
    };

    return GlyphIterator();
}

GlyphIterator ZepBuffer::FindOnLineMotion(GlyphIterator start, const uint8_t* pCh, SearchDirection dir) const
{
    auto entry = start;
    auto IsMatch = [pCh](const char ch) {
        if (*pCh == ch)
            return true;
        return false;
    };
    auto NotMatchNotEnd = [pCh](const char ch) {
        if (*pCh != ch && ch != '\n')
            return true;
        return false;
    };

    if (dir == SearchDirection::Forward)
    {
        SkipOne(IsMatch, start, dir);
        Skip(NotMatchNotEnd, start, dir);
    }
    else
    {
        SkipOne(IsMatch, start, dir);
        Skip(NotMatchNotEnd, start, dir);
    }

    if (start.Valid() && *pCh == start.Char())
    {
        return start;
    }
    return entry;
}

GlyphIterator ZepBuffer::WordMotion(GlyphIterator start, uint32_t searchType, SearchDirection dir) const
{
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    MotionBegin(start);

    if (dir == SearchDirection::Forward)
    {
        if (Skip(IsWord, start, dir))
        {
            // Skipped a word, skip spaces then done
            Skip(IsSpaceOrTerminal, start, dir);
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

GlyphIterator ZepBuffer::EndWordMotion(GlyphIterator start, uint32_t searchType, SearchDirection dir) const
{
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;

    MotionBegin(start);

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
        Skip(IsSpaceOrNewline, start, dir);

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

GlyphIterator ZepBuffer::ChangeWordMotion(GlyphIterator start, uint32_t searchType, SearchDirection dir) const
{
    // Change word is different to work skipping; it will change a string of spaces, for example.
    // Essentially it changes 'what you are over', based on the word rule
    auto IsWord = searchType == SearchType::Word ? IsWordChar : IsWORDChar;
    MotionBegin(start);
    if (Skip(IsWord, start, dir))
    {
        return start;
    }
    SkipNot(IsWord, start, dir);
    return start;
}

GlyphIterator ZepBuffer::ClampToVisibleLine(GlyphIterator in) const
{
    in = in.Clamp();
    auto loc = GetLinePos(in, LineLocation::LineLastNonCR);
    in = std::min(loc, in);
    return in;
}

// Method for querying the beginning and end of a line
bool ZepBuffer::GetLineOffsets(const long line, ByteRange& range) const
{
    // Not valid
    if ((long)m_lineEnds.size() <= line)
    {
        range.first = 0;
        range.second = 0;
        return false;
    }

    // Find the line bounds - we know the end, find the start from the previous
    range.second = m_lineEnds[line];
    range.first = line == 0 ? 0 : m_lineEnds[line - 1];
    return true;
}

std::string ZepBuffer::GetFileExtension() const
{
    std::string ext;
    if (GetFilePath().has_filename() && GetFilePath().filename().has_extension())
    {
        ext = string_tolower(GetFilePath().filename().extension().string());
    }
    else
    {
        auto str = GetName();
        size_t dot_pos = str.find_last_of(".");
        if (dot_pos != std::string::npos)
        {
            ext = string_tolower(str.substr(dot_pos, str.length() - dot_pos));
        }
    }
    return ext;
}

// Basic load support; read a file if it's present, but keep
// the file path in case you want to write later
void ZepBuffer::Load(const ZepPath& path)
{
    // Set the name from the path
    if (path.has_filename())
    {
        m_strName = path.filename().string();
    }
    else
    {
        m_strName = m_filePath.string();
    }

    // Must set the syntax before the first buffer change messages
    // TODO: I believe that some of this buffer config should move to Editor.cpp
    GetEditor().SetBufferSyntax(*this);

    if (GetEditor().GetFileSystem().Exists(path))
    {
        m_filePath = GetEditor().GetFileSystem().Canonical(path);
        auto read = GetEditor().GetFileSystem().Read(path);

        // Always set text, to ensure we prepare the buffer with 0 terminator,
        // even if string is empty
        SetText(read, true);
    }
    else
    {
        // Can't canonicalize a non-existent path.
        // But we may have a path we haven't save to yet!
        Clear();
        m_filePath = path;
    }
}

bool ZepBuffer::Save(int64_t& size)
{
    if (ZTestFlags(m_fileFlags, FileFlags::Locked))
    {
        return false;
    }

    if (ZTestFlags(m_fileFlags, FileFlags::ReadOnly))
    {
        return false;
    }

    auto str = GetGapBuffer().string();

    // Put back /r/n if necessary while writing the file
    // At the moment, Zep removes /r/n and just uses /n while modifying text.
    // It replaces the /r on files that had it afterwards
    // Alternatively we could manage them 'in place', but that would make parsing more complex.
    // And then what do you do if there are 2 different styles in the file.
    if (m_fileFlags & FileFlags::StrippedCR)
    {
        // TODO: faster way to replace newlines
        string_replace_in_place(str, "\n", "\r\n");
    }

    // Remove the appended 0 if necessary
    size = (int64_t)str.size();
    if (m_fileFlags & FileFlags::TerminatedWithZero)
    {
        size--;
    }

    if (size <= 0)
    {
        return true;
    }

    if (GetEditor().GetFileSystem().Write(m_filePath, &str[0], (size_t)size))
    {
        m_fileFlags = ZClearFlags(m_fileFlags, FileFlags::Dirty);
        return true;
    }
    return false;
}

std::string ZepBuffer::GetDisplayName() const
{
    if (m_filePath.empty())
    {
        return m_strName;
    }
    return m_filePath.string();
}

ZepPath ZepBuffer::GetFilePath() const
{
    return m_filePath;
}

void ZepBuffer::SetFilePath(const ZepPath& path)
{
    auto testPath = path;
    if (GetEditor().GetFileSystem().Exists(testPath))
    {
        testPath = GetEditor().GetFileSystem().Canonical(testPath);
    }

    if (!GetEditor().GetFileSystem().Equivalent(testPath, m_filePath))
    {
        m_filePath = testPath;
    }
    GetEditor().SetBufferSyntax(*this);
}

// Remember that we updated the buffer and dirty the state
// Clients can use these values to figure out update times and dirty state
void ZepBuffer::MarkUpdate()
{
    m_updateCount++;
    m_lastUpdateTime = timer_get_time_now();

    m_fileFlags = ZSetFlags(m_fileFlags, FileFlags::Dirty);
}

// Clear this buffer.  If it was previously not clear, it has been updated.
// Otherwise it is just reset to default state.  A new buffer is always initially cleared.
void ZepBuffer::Clear()
{
    // A buffer that is empty is brand new; just make it 0 chars and return
    if (m_gapBuffer.size() <= 1)
    {
        m_gapBuffer.clear();
        m_gapBuffer.push_back(0);
        m_lineEnds.clear();
        m_fileFlags = ZSetFlags(m_fileFlags, FileFlags::TerminatedWithZero);
        m_lineEnds.push_back(End().Index() + 1);
        return;
    }

    // Inform clients we are about to change the buffer
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, GlyphIterator(this), End()));

    m_gapBuffer.clear();
    m_gapBuffer.push_back(0);
    m_lineEnds.clear();
    m_fileFlags = ZSetFlags(m_fileFlags, FileFlags::TerminatedWithZero);
    m_lineEnds.push_back(End().Index() + 1);

    {
        MarkUpdate();
        GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextDeleted, GlyphIterator(this), End()));
    }
}

// Replace the buffer with the text
void ZepBuffer::SetText(const std::string& text, bool initFromFile)
{
    // First, clear it
    Clear();

    bool lastWasSpace = false;
    if (!text.empty())
    {
        // Since incremental insertion of a big file into a gap buffer gives us worst case performance,
        // We build the buffer in a separate array and assign it.  Much faster.
        std::vector<uint8_t> input;

        m_lineEnds.clear();

        // Update the gap buffer with the text
        // We remove \r, we only care about \n
        for (auto& ch : text)
        {
            if (ch == '\r')
            {
                m_fileFlags |= FileFlags::StrippedCR;
            }
            else
            {
                input.push_back(ch);
                if (ch == '\n')
                {
                    m_lineEnds.push_back(ByteIndex(input.size()));
                    lastWasSpace = false;
                }
                else if (ch == '\t')
                {
                    m_fileFlags |= FileFlags::HasTabs;
                    lastWasSpace = false;
                }
                else if (ch == ' ')
                {
                    if (lastWasSpace)
                    {
                        m_fileFlags |= FileFlags::HasSpaceTabs;
                    }
                    lastWasSpace = true;
                }
                else
                {
                    lastWasSpace = false;
                }
            }
        }
        m_gapBuffer.assign(input.begin(), input.end());
    }

    // If file is only tabs, then force tab mode
    if (HasFileFlags(FileFlags::HasTabs) && !HasFileFlags(FileFlags::HasSpaceTabs))
    {
        m_fileFlags = ZSetFlags(m_fileFlags, FileFlags::InsertTabs);
    }

    if (m_gapBuffer[m_gapBuffer.size() - 1] != 0)
    {
        m_fileFlags |= FileFlags::TerminatedWithZero;
        m_gapBuffer.push_back(0);
    }

    // TODO: Why is a line end needed always?
    // TODO: Line ends 1 beyond, or just for end?  Can't remember this detail:
    // understand it, then write a unit test to ensure it.
    m_lineEnds.push_back(End().Index() + 1);

    MarkUpdate();

    // When loading a file, send the Loaded message to distinguish it from adding to a buffer, and remember that the buffer is not dirty in this case
    if (initFromFile)
    {
        GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::Loaded, Begin(), End()));

        // Doc is not dirty
        m_fileFlags = ZClearFlags(m_fileFlags, FileFlags::Dirty);
    }
    else
    {
        GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextAdded, Begin(), End()));
    }
}

// TODO: This can be cleaner
// The function needs to find the point on the line which bufferLocation is on.
// It needs to account for empty lines or the last line, zero terminated.
// It shouldn't walk away to another line!
GlyphIterator ZepBuffer::GetLinePos(GlyphIterator bufferLocation, LineLocation lineLocation) const
{
    if (lineLocation == LineLocation::None)
    {
        assert(!"Invalid");
        return GlyphIterator();
    }

    bufferLocation.Clamp();
    if (m_gapBuffer.empty())
    {
        return bufferLocation;
    }

    GlyphIterator itr = bufferLocation;
    GlyphIterator itrBegin = Begin();
    GlyphIterator itrEnd = End();

    GlyphIterator itrLineStart(itr);

    // If we are on the CR, move back 1, unless the \n is all that is on the line
    if (itrLineStart != itrBegin)
    {
        if (itrLineStart.Char() == '\n')
        {
            itrLineStart.Move(-1);
        }

        // Find the end of the previous line
        while (itrLineStart > itrBegin && itrLineStart.Char() != '\n')
        {
            itrLineStart.Move(-1);
        }

        if (itrLineStart.Char() == '\n')
            itrLineStart.Move(1);

        itr = itrLineStart;
    }

    switch (lineLocation)
    {
    default:
    // We are on the first bit of the line anyway
    case LineLocation::LineBegin:
    {
        return itr.Clamped();
    }
    break;

    // The point just after the line end
    case LineLocation::BeyondLineEnd:
    {
        while (itr < itrEnd && itr.Char() != '\n' && itr.Char() != 0)
        {
            itr.Move(1);
        }
        itr.Move(1);
        return itr.Clamped();
    }
    break;

    case LineLocation::LineCRBegin:
    {
        while (itr < itrEnd
            && itr.Char() != '\n'
            && itr.Char() != 0)
        {
            itr.Move(1);
        }
        return itr;
    }
    break;

    case LineLocation::LineFirstGraphChar:
    {
        while (itr < itrEnd && !std::isgraph(ToASCII(itr.Char())) && itr.Char() != '\n')
        {
            itr.Move(1);
        }
        return itr.Clamped();
    }
    break;

    case LineLocation::LineLastNonCR:
    {
        auto itrFirst = itr;

        while (itr < itrEnd
            && itr.Char() != '\n'
            && itr.Char() != 0)
        {
            itr.Move(1);
        }

        if (itrFirst != itr)
        {
            itr.Move(-1);
        }

        return itr.Clamped();
    }
    break;

    case LineLocation::LineLastGraphChar:
    {
        while (itr < itrEnd
            && itr.Char() != '\n'
            && itr.Char() != 0)
        {
            itr.Move(1);
        }

        while (itr > itrBegin && itr < itrEnd
            && !std::isgraph(ToASCII(itr.Char())))
        {
            itr.Move(-1);
        }

        if (itr < itrLineStart)
        {
            itr = itrLineStart;
        }

        return itr.Clamped();
    }
    break;
    }
}

void ZepBuffer::UpdateForDelete(const GlyphIterator& startItr, const GlyphIterator& endItr)
{
    ForEachMarker(RangeMarkerType::All, Zep::SearchDirection::Forward, startItr, End(), [&](const std::shared_ptr<RangeMarker>& marker) {
        if (startItr.Index() > marker->range.second)
        {
            return true;
        }
        else
        {
            auto distance = endItr.Index() - startItr.Index();
            marker->range.first -= distance;
            marker->range.second -= distance;

            marker->range.first = std::clamp(marker->range.first, 0l, End().Index());
            marker->range.second = std::clamp(marker->range.second, 0l, End().Index());
        }
        return true;
    });
}

void ZepBuffer::UpdateForInsert(const GlyphIterator& startItr, const GlyphIterator& endItr)
{
    ForEachMarker(RangeMarkerType::All, SearchDirection::Forward, startItr, End(), [&](const std::shared_ptr<RangeMarker>& marker) {
        if (startItr.Index() > marker->range.second)
        {
            return true;
        }
        else 
        {
            auto distance = endItr.Index() - startItr.Index();
            marker->range.first += distance;
            marker->range.second += distance;

            marker->range.first = std::clamp(marker->range.first, 0l, End().Index());
            marker->range.second = std::clamp(marker->range.second, 0l, End().Index());
        }
        return true;
    });
}

bool ZepBuffer::Insert(const GlyphIterator& startIndex, const std::string& str)
{
    if (!startIndex.Valid())
    {
        return false;
    }

    GlyphIterator endIndex(this, startIndex.Index() + long(str.length()));

    UpdateForInsert(startIndex, endIndex);

    // We are about to modify this range
    // TODO: Is this correct, and/or useful in any way??
    // We aren't changing this range at all; we are shifting those characters forward and replacing the area
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startIndex, endIndex));

    // abcdef\r\nabc<insert>dfdf\r\n
    auto itrLine = std::lower_bound(m_lineEnds.begin(), m_lineEnds.end(), startIndex.Index());
    ;
    if (itrLine != m_lineEnds.end() && *itrLine <= startIndex.Index())
    {
        itrLine++;
    }

    auto itrEnd = str.end();
    auto itrBegin = str.begin();
    auto itr = str.begin();

    // Make a list of lines to 'insert'
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
            lines.push_back(long(itr - itrBegin) + startIndex.Index());
        }
    }

    // Increment the rest of the line ends
    // We make all the remaning line ends bigger by the fixed_size of the insertion
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

    m_gapBuffer.insert(m_gapBuffer.begin() + startIndex.Index(), str.begin(), str.end());

    MarkUpdate();

    // This is the range we added (not valid any more in the buffer)
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextAdded, startIndex, endIndex));

    return true;
}

bool ZepBuffer::Replace(const GlyphIterator& startIndex, const GlyphIterator& endIndex, const std::string& str)
{
    if (!startIndex.Valid() || !endIndex.Valid())
    {
        return false;
    }

    // We are about to modify this range
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startIndex, endIndex));

    // Perform a straight replace
    for (auto loc = startIndex; loc < endIndex; loc++)
    {
        // Note we don't support utf8 yet
        // TODO: (0) Broken now we support utf8
        m_gapBuffer[loc.Index()] = str[0];
    }

    MarkUpdate();

    // This is the range we added (not valid any more in the buffer)
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextChanged, startIndex, endIndex));

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
bool ZepBuffer::Delete(const GlyphIterator& startIndex, const GlyphIterator& endIndex)
{
    assert(startIndex.Valid());
    assert(endIndex.Valid());

    // We are about to modify this range
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::PreBufferChange, startIndex, endIndex));

    UpdateForDelete(startIndex, endIndex);

    auto itrLine = std::lower_bound(m_lineEnds.begin(), m_lineEnds.end(), startIndex.Index());
    if (itrLine == m_lineEnds.end())
    {
        return false;
    }

    auto itrLastLine = std::upper_bound(itrLine, m_lineEnds.end(), endIndex.Index());
    auto offsetDiff = endIndex.Index() - startIndex.Index();

    if (*itrLine <= startIndex.Index())
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

    m_gapBuffer.erase(m_gapBuffer.begin() + startIndex.Index(), m_gapBuffer.begin() + endIndex.Index());
    assert(m_gapBuffer.size() > 0 && m_gapBuffer[m_gapBuffer.size() - 1] == 0);

    MarkUpdate();

    // This is the range we deleted (not valid any more in the buffer)
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::TextDeleted, startIndex, endIndex));

    return true;
}

ZepTheme& ZepBuffer::GetTheme() const
{
    if (m_spOverrideTheme)
    {
        return *m_spOverrideTheme;
    }
    return GetEditor().GetTheme();
}

void ZepBuffer::SetTheme(std::shared_ptr<ZepTheme> spTheme)
{
    m_spOverrideTheme = spTheme;
}

bool ZepBuffer::HasSelection() const
{
    return m_selection.first != m_selection.second;
}

void ZepBuffer::ClearSelection()
{
    m_selection.first = Begin();
    m_selection.second = Begin();
}

GlyphRange ZepBuffer::GetSelection() const
{
    return m_selection;
}

void ZepBuffer::SetSelection(const GlyphRange& selection)
{
    m_selection = selection;
    if (m_selection.first > m_selection.second)
    {
        std::swap(m_selection.first, m_selection.second);
    }
}

void ZepBuffer::AddRangeMarker(std::shared_ptr<RangeMarker> spMarker)
{
    m_rangeMarkers[spMarker->range.first].insert(spMarker);

    // TODO: Why is this necessary; marks the whole buffer
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::MarkersChanged, Begin(), End()));
}

void ZepBuffer::ClearRangeMarker(std::shared_ptr<RangeMarker> spMarker)
{
    std::set<ByteIndex> emptyLocations;
    for (auto& markerPair : m_rangeMarkers)
    {
        markerPair.second.erase(spMarker);
        if (markerPair.second.empty())
        {
            emptyLocations.insert(markerPair.first);
        }
    }

    for (auto& victim : emptyLocations)
    {
        m_rangeMarkers.erase(victim);
    }
}

void ZepBuffer::ClearRangeMarkers(const std::set<std::shared_ptr<RangeMarker>>& markers)
{
    for (auto& marker : markers)
    {
        ClearRangeMarker(marker);
    }
    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::MarkersChanged, Begin(), End()));
}

void ZepBuffer::ClearRangeMarkers(uint32_t markerType)
{
    std::set<std::shared_ptr<RangeMarker>> markers;
    ForEachMarker(markerType, SearchDirection::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker>& pMarker) {
        markers.insert(pMarker);
        return true;
    });

    for (auto& victim : markers)
    {
        ClearRangeMarker(victim);
    }

    GetEditor().Broadcast(std::make_shared<BufferMessage>(this, BufferMessageType::MarkersChanged, Begin(), End()));
}

void ZepBuffer::ForEachMarker(uint32_t markerType, SearchDirection dir, const GlyphIterator& begin, const GlyphIterator& end, std::function<bool(const std::shared_ptr<RangeMarker>&)> fnCB) const
{
    //auto itrStart = m_rangeMarkers.lower_bound(begin.Index());
    //if (itrStart == m_rangeMarkers.end())
    //    return;

    //auto itrEnd = m_rangeMarkers.upper_bound(end.Index());

    if (dir == SearchDirection::Forward)
    {
        for (auto itr = m_rangeMarkers.begin(); itr != m_rangeMarkers.end(); itr++)
        {
            for (auto& markerItem : itr->second)
            {
                if ((markerItem->markerType & markerType) == 0)
                {
                    continue;
                }

                // TODO: This shouldn't be necessary, but the bound above is not working as expected!
                if (markerItem->range.second < begin.Index() ||
                    markerItem->range.first >= end.Index())
                {
                    continue;
                }

                if (!fnCB(markerItem))
                {
                    return;
                }
            }
        }
    }
    else
    {
        auto itrREnd = std::make_reverse_iterator(m_rangeMarkers.begin());
        auto itrRStart = std::make_reverse_iterator(m_rangeMarkers.end());

        for (auto itr = itrRStart; itr != itrREnd; itr++)
        {
            for (auto& markerItem : itr->second)
            {
                if ((markerItem->markerType & markerType) == 0)
                {
                    continue;
                }
                if (!fnCB(markerItem))
                {
                    return;
                }
            }
        }
    }
}

void ZepBuffer::HideMarkers(uint32_t markerType)
{
    ForEachMarker(markerType, SearchDirection::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker>& spMarker) {
        if ((spMarker->markerType & markerType) != 0)
        {
            spMarker->displayType = RangeMarkerDisplayType::Hidden;
        }
        return true;
    });
}

void ZepBuffer::ShowMarkers(uint32_t markerType, uint32_t displayType)
{
    ForEachMarker(markerType, SearchDirection::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker>& spMarker) {
        if ((spMarker->markerType & markerType) != 0)
        {
            spMarker->displayType = displayType;
        }
        return true;
    });
}

tRangeMarkers ZepBuffer::GetRangeMarkers(uint32_t markerType) const
{
    tRangeMarkers markers;
    ForEachMarker(markerType, SearchDirection::Forward, Begin(), End(), [&](const std::shared_ptr<RangeMarker>& spMarker) {
        if ((spMarker->markerType & markerType) != 0)
        {
            markers[spMarker->range.first].insert(spMarker);
        }
        return true;
    });
    return markers;
}

std::shared_ptr<RangeMarker> ZepBuffer::FindNextMarker(GlyphIterator start, SearchDirection dir, uint32_t markerType)
{
    start.Clamp();

    std::shared_ptr<RangeMarker> spFound;
    auto search = [&]() {
        ForEachMarker(markerType, dir, Begin(), End(), [&](const std::shared_ptr<RangeMarker>& marker) {
            if (dir == SearchDirection::Forward)
            {
                if (marker->range.first <= start.Index())
                {
                    return true;
                }
            }
            else
            {
                if (marker->range.first >= start.Index())
                {
                    return true;
                }
            }

            spFound = marker;
            return false;
        });
    };

    search();
    if (spFound == nullptr)
    {
        // Wrap
        start = (dir == SearchDirection::Forward ? Begin() : End());
        search();
    }
    return spFound;
}

void ZepBuffer::SetBufferType(BufferType type)
{
    m_bufferType = type;
}

BufferType ZepBuffer::GetBufferType() const
{
    return m_bufferType;
}

void ZepBuffer::SetLastEditLocation(GlyphIterator loc)
{
    m_lastEditLocation = loc;
}

GlyphIterator ZepBuffer::GetLastEditLocation()
{
    if (!m_lastEditLocation.Valid())
    {
        m_lastEditLocation = GlyphIterator(this, 0);
    }
    return m_lastEditLocation;
}

ZepMode* ZepBuffer::GetMode() const
{
    if (m_spMode)
    {
        return m_spMode.get();
    }
    return GetEditor().GetGlobalMode();
}

void ZepBuffer::SetMode(std::shared_ptr<ZepMode> spMode)
{
    m_spMode = spMode;
}

tRangeMarkers ZepBuffer::GetRangeMarkersOnLine(long line) const
{
    ByteRange range;
    GetLineOffsets(line, range);

    tRangeMarkers rangeMarkers;
    ForEachMarker(RangeMarkerType::All,
        Zep::SearchDirection::Forward,
        GlyphIterator(this, range.first), GlyphIterator(this, range.second),
        [&](const std::shared_ptr<RangeMarker>& marker) {
            rangeMarkers[marker->range.first].insert(marker);
            return true;
        });
    return rangeMarkers;
}

bool ZepBuffer::IsHidden() const
{
    auto windows = GetEditor().FindBufferWindows(this);
    return windows.empty();
}

void ZepBuffer::SetFileFlags(uint32_t flags, bool set)
{
    m_fileFlags = ZSetFlags(m_fileFlags, flags, set);
}

void ZepBuffer::ClearFileFlags(uint32_t flags)
{
    m_fileFlags = ZSetFlags(m_fileFlags, flags, false);
}

bool ZepBuffer::HasFileFlags(uint32_t flags) const
{
    return ZTestFlags(m_fileFlags, flags);
}

void ZepBuffer::ToggleFileFlag(uint32_t flags)
{
    m_fileFlags = ZSetFlags(m_fileFlags, flags, !ZTestFlags(m_fileFlags, flags));
}

NVec2i ZepBuffer::GetExpression(ExpressionType type, const GlyphIterator location, const std::vector<char>& beginExpression, const std::vector<char>& endExpression) const
{
    GlyphIterator itr = Begin();
    GlyphIterator itrEnd = End();

    int maxDepth = -1;
    struct Expression
    {
        int depth = 0;
        NVec2i range = NVec2i(0);
        std::vector<std::shared_ptr<Expression>> children;
        Expression* pParent = nullptr;
    };

    std::vector<std::shared_ptr<Expression>> topLevel;

    Expression* pCurrent = nullptr;
    Expression* pInner = nullptr;

    while (itr != itrEnd)
    {
        auto ch = itr.Char();

        for (auto& exp : beginExpression)
        {
            if (exp == ch)
            {
                auto spChild = std::make_shared<Expression>();
                if (pCurrent)
                {
                    pCurrent->children.push_back(spChild);
                    spChild->pParent = pCurrent;
                    spChild->depth = pCurrent->depth + 1;
                }
                else
                {
                    topLevel.push_back(spChild);
                }
                pCurrent = spChild.get();
                pCurrent->range.x = itr.Index();
            }
        }

        for (auto& exp : endExpression)
        {
            if (exp == ch)
            {
                if (pCurrent)
                {
                    pCurrent->range.y = itr.Index() + 1;

                    // Check the sub exp
                    if ((pCurrent->range.x <= location.Index()) && (pCurrent->range.y > location.Index()))
                    {
                        if (pCurrent->depth > maxDepth)
                        {
                            maxDepth = pCurrent->depth;
                            pInner = pCurrent;
                        }
                    }
                    pCurrent = pCurrent->pParent;
                }
            }
        }

        itr.Move(1);
    }

    if (type == ExpressionType::Inner)
    {
        if (pInner)
        {
            return pInner->range;
        }
        return NVec2i(0, 0);
    }

    Expression* pBest = nullptr;
    int dist = std::numeric_limits<int>::max();

    for (auto& outer : topLevel)
    {
        if (location.Index() >= outer->range.x && location.Index() < outer->range.y)
        {
            return outer->range;
        }
        else
        {
            auto leftDist = std::abs(outer->range.x - location.Index());
            auto rightDist = std::abs(location.Index() - outer->range.y);
            if (leftDist < dist)
            {
                pBest = outer.get();
                dist = leftDist;
            }
            if (rightDist < dist)
            {
                pBest = outer.get();
                dist = rightDist;
            }
        }
    }

    if (pBest)
    {
        return pBest->range;
    }

    return NVec2i(0, 0);
}

GlyphIterator ZepBuffer::End() const
{
    return GlyphIterator(this, std::max(0l, long(m_gapBuffer.size() - 1)));
}

GlyphIterator ZepBuffer::Begin() const
{
    return GlyphIterator(this);
}

} // namespace Zep
