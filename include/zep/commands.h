#pragma once

#include "zep/buffer.h"

namespace Zep
{

class ZepCommand
{
public:
    ZepCommand(ZepBuffer& currentMode, ByteIndex cursorBefore = -1, ByteIndex cursorAfter = -1)
        : m_buffer(currentMode)
        , m_cursorBefore(cursorBefore)
        , m_cursorAfter(cursorAfter)
    {
    }

    virtual ~ZepCommand()
    {
    }

    virtual void Redo() = 0;
    virtual void Undo() = 0;

    virtual ByteIndex GetCursorAfter() const
    {
        return m_cursorAfter;
    }
    virtual ByteIndex GetCursorBefore() const
    {
        return m_cursorBefore;
    }

protected:
    ZepBuffer& m_buffer;
    ByteIndex m_cursorBefore = -1;
    ByteIndex m_cursorAfter = -1;
};

class ZepCommand_GroupMarker : public ZepCommand
{
public:
    ZepCommand_GroupMarker(ZepBuffer& currentMode) : ZepCommand(currentMode) {}
    virtual void Redo() override {};
    virtual void Undo() override {};
};

class ZepCommand_EndGroup : public ZepCommand
{
public:
    ZepCommand_EndGroup(ZepBuffer& currentMode) : ZepCommand(currentMode) {}
    virtual void Redo() override {};
    virtual void Undo() override {};
};

class ZepCommand_DeleteRange : public ZepCommand
{
public:
    ZepCommand_DeleteRange(ZepBuffer& buffer, const ByteIndex& startIndex, const ByteIndex& endIndex, const ByteIndex& cursor = ByteIndex{-1}, const ByteIndex& cursorAfter = ByteIndex{-1});
    virtual ~ZepCommand_DeleteRange(){};

    virtual void Redo() override;
    virtual void Undo() override;

    ByteIndex m_startIndex;
    ByteIndex m_endIndex;

    std::string m_deleted;
};

enum class ReplaceRangeMode
{
    Fill,
    Replace,
};

class ZepCommand_ReplaceRange : public ZepCommand
{
public:
    ZepCommand_ReplaceRange(ZepBuffer& buffer, ReplaceRangeMode replaceMode, const ByteIndex& startIndex, const ByteIndex& endIndex, const std::string& ch, const ByteIndex& cursor = ByteIndex{-1}, const ByteIndex& cursorAfter = ByteIndex{-1});
    virtual ~ZepCommand_ReplaceRange(){};

    virtual void Redo() override;
    virtual void Undo() override;

    ByteIndex m_startIndex;
    ByteIndex m_endIndex;

    std::string m_strDeleted;
    std::string m_strReplace;
    ReplaceRangeMode m_mode;
};

class ZepCommand_Insert : public ZepCommand
{
public:
    ZepCommand_Insert(ZepBuffer& buffer, const ByteIndex& startIndex, const std::string& str, const ByteIndex& cursor = ByteIndex{-1}, const ByteIndex& cursorAfter = ByteIndex{-1});
    virtual ~ZepCommand_Insert(){};

    virtual void Redo() override;
    virtual void Undo() override;

    ByteIndex m_startIndex;
    std::string m_strInsert;

    ByteIndex m_endIndexInserted = -1;
};

} // namespace Zep
