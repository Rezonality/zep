#include "zep/commands.h"

namespace Zep
{

// Delete Range of chars
ZepCommand_DeleteRange::ZepCommand_DeleteRange(ZepBuffer& buffer, const GlyphIterator& start, const GlyphIterator& end, const GlyphIterator& cursor, const GlyphIterator& cursorAfter)
    : ZepCommand(buffer, cursor, cursorAfter.Valid() ? cursorAfter : start)
    , m_startIndex(start)
    , m_endIndex(end)
{
    assert(m_startIndex.Valid());
    assert(m_endIndex.Valid());

    // We never allow deletion of the '0' at the end of the buffer
    if (buffer.GetGapBuffer().empty())
    {
        m_endIndex = m_startIndex;
    }
    else
    {
        m_endIndex.Clamp();
    }
}

void ZepCommand_DeleteRange::Redo()
{
    if (m_startIndex != m_endIndex)
    {
        // TODO: Helper function to extract string from glyph range
        m_deleted = std::string(m_buffer.GetGapBuffer().begin() + m_startIndex.Index(), m_buffer.GetGapBuffer().begin() + m_endIndex.Index());

        m_buffer.Delete(m_startIndex, m_endIndex);
    }
}

void ZepCommand_DeleteRange::Undo()
{
    if (m_deleted.empty())
        return;
    m_buffer.Insert(m_startIndex, m_deleted);
}

// Insert a string
ZepCommand_Insert::ZepCommand_Insert(ZepBuffer& buffer, const GlyphIterator& start, const std::string& str, const GlyphIterator& cursor, const GlyphIterator& cursorAfter)
    : ZepCommand(buffer, cursor, cursorAfter.Valid() ? cursorAfter : (start.PeekByteOffset(long(str.size()))))
    , m_startIndex(start)
    , m_strInsert(str)
{
    m_startIndex.Clamp();
}

void ZepCommand_Insert::Redo()
{
    bool ret = m_buffer.Insert(m_startIndex, m_strInsert);
    assert(ret);
    if (ret == true)
    {
        m_endIndexInserted = m_startIndex.PeekByteOffset(long(m_strInsert.size()));
    }
    else
    {
        m_endIndexInserted.Invalidate();
    }
}

void ZepCommand_Insert::Undo()
{
    if (m_endIndexInserted.Valid())
    {
        m_buffer.Delete(m_startIndex, m_endIndexInserted);
    }
}

// Replace
ZepCommand_ReplaceRange::ZepCommand_ReplaceRange(ZepBuffer& buffer, ReplaceRangeMode currentMode, const GlyphIterator& startIndex, const GlyphIterator& endIndex, const std::string& strReplace, const GlyphIterator& cursor, const GlyphIterator& cursorAfter)
    : ZepCommand(buffer, cursor.Valid() ? cursor : endIndex, cursorAfter.Valid() ? cursorAfter : startIndex)
    , m_startIndex(startIndex)
    , m_endIndex(endIndex)
    , m_strReplace(strReplace)
    , m_mode(currentMode)
{
    m_startIndex.Clamp();
}

void ZepCommand_ReplaceRange::Redo()
{
    if (m_startIndex != m_endIndex)
    {
        m_strDeleted = std::string(m_buffer.GetGapBuffer().begin() + m_startIndex.Index(), m_buffer.GetGapBuffer().begin() + m_endIndex.Index());
        if (m_mode == ReplaceRangeMode::Fill)
        {
            m_buffer.Replace(m_startIndex, m_endIndex, m_strReplace);
        }
        else
        {
            m_buffer.Delete(m_startIndex, m_endIndex);
            m_buffer.Insert(m_startIndex, m_strReplace);
        }
    }
}

void ZepCommand_ReplaceRange::Undo()
{
    if (m_startIndex != m_endIndex)
    {
        if (m_mode == ReplaceRangeMode::Fill)
        {
            m_buffer.Delete(m_startIndex, m_endIndex);
            m_buffer.Insert(m_startIndex, m_strDeleted);
        }
        else
        {
            // Delete the previous inserted text
            m_buffer.Delete(m_startIndex, m_startIndex.PeekByteOffset((long)m_strReplace.length()));
            // Insert the deleted text
            m_buffer.Insert(m_startIndex, m_strDeleted);
        }
    }
}

} // namespace Zep