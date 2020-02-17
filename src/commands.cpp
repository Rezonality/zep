#include "zep/commands.h"

namespace Zep
{

// Delete Range of chars
ZepCommand_DeleteRange::ZepCommand_DeleteRange(ZepBuffer& buffer, const ByteIndex& start, const ByteIndex& end, const ByteIndex& cursor, const ByteIndex& cursorAfter)
    : ZepCommand(buffer, cursor, cursorAfter != -1 ? cursorAfter : start)
    , m_startIndex(start)
    , m_endIndex(end)
{
    // We never allow deletion of the '0' at the end of the buffer
    if (buffer.GetText().empty())
    {
        m_endIndex = m_startIndex;
    }
    else
    {
        m_endIndex = std::min(m_endIndex, long(buffer.GetText().size()) - 1l);
    }
}

void ZepCommand_DeleteRange::Redo()
{
    if (m_startIndex != m_endIndex)
    {
        m_deleted = std::string(m_buffer.GetText().begin() + m_startIndex, m_buffer.GetText().begin() + m_endIndex);

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
ZepCommand_Insert::ZepCommand_Insert(ZepBuffer& buffer, const ByteIndex& start, const std::string& str, const ByteIndex& cursor, const ByteIndex& cursorAfter)
    : ZepCommand(buffer, cursor, cursorAfter != -1 ? cursorAfter : (start + long(str.length())))
    , m_startIndex(start)
    , m_strInsert(str)
{
    m_startIndex = buffer.Clamp(m_startIndex);
}

void ZepCommand_Insert::Redo()
{
    bool ret = m_buffer.Insert(m_startIndex, m_strInsert);
    assert(ret);
    if (ret == true)
    {
        m_endIndexInserted = m_startIndex + ByteIndex(m_strInsert.size());
    }
    else
    {
        m_endIndexInserted = -1;
    }
}

void ZepCommand_Insert::Undo()
{
    if (m_endIndexInserted != -1)
    {
        m_buffer.Delete(m_startIndex, m_endIndexInserted);
    }
}

// Replace
ZepCommand_ReplaceRange::ZepCommand_ReplaceRange(ZepBuffer& buffer, ReplaceRangeMode currentMode, const ByteIndex& startIndex, const ByteIndex& endIndex, const std::string& strReplace, const ByteIndex& cursor, const ByteIndex& cursorAfter)
    : ZepCommand(buffer, cursor != -1 ? cursor : endIndex, cursorAfter != -1 ? cursorAfter : startIndex)
    , m_startIndex(startIndex)
    , m_endIndex(endIndex)
    , m_strReplace(strReplace)
    , m_mode(currentMode)
{
    m_startIndex = buffer.Clamp(m_startIndex);
}

void ZepCommand_ReplaceRange::Redo()
{
    if (m_startIndex != m_endIndex)
    {
        m_strDeleted = std::string(m_buffer.GetText().begin() + m_startIndex, m_buffer.GetText().begin() + m_endIndex);
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
            m_buffer.Delete(m_startIndex, m_startIndex + (long)m_strReplace.length());
            // Insert the deleted text
            m_buffer.Insert(m_startIndex, m_strDeleted);
        }
    }
}

} // namespace Zep