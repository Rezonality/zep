#pragma once

#include "mode.h"

namespace Zep
{

namespace CommandFlags
{
enum
{
    GroupBoundary = (1 << 0),
};

}

class ZepCommand
{
public:
    ZepCommand(ZepBuffer& mode, BufferLocation cursorAfter = -1)
        : m_buffer(mode),
        m_cursorAfter(cursorAfter)
    {}

    virtual ~ZepCommand() {}

    virtual void Redo() = 0;
    virtual void Undo() = 0;

    virtual void SetFlags(uint32_t flags) { m_flags = flags; }
    virtual uint32_t GetFlags() const { return m_flags; }
    virtual BufferLocation GetCursorAfter() const { return m_cursorAfter; }

protected:
    ZepBuffer& m_buffer;
    uint32_t m_flags = 0;
    BufferLocation m_cursorAfter = -1;
};

class ZepCommand_DeleteRange : public ZepCommand
{
public:
    ZepCommand_DeleteRange(ZepBuffer& buffer, const BufferLocation& startOffset, const BufferLocation& endOffset, const BufferLocation& cursorAfter = BufferLocation{ -1 });
    virtual ~ZepCommand_DeleteRange() {};

    virtual void Redo() override;
    virtual void Undo() override;

    BufferLocation m_startOffset;
    BufferLocation m_endOffset;

    std::string m_deleted;
};

class ZepCommand_Insert : public ZepCommand
{
public:
    ZepCommand_Insert(ZepBuffer& buffer, const BufferLocation& startOffset, const std::string& str, const BufferLocation& cursorAfter = BufferLocation{ -1 });
    virtual ~ZepCommand_Insert() {};

    virtual void Redo() override;
    virtual void Undo() override;

    BufferLocation m_startOffset;
    std::string m_strInsert;

    BufferLocation m_endOffsetInserted;
};

} // Zep