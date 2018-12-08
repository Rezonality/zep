#include <sstream>
#include <cctype>
#include <cmath>

#include "display.h"
#include "tab_window.h"
#include "window.h"
#include "syntax.h"
#include "buffer.h"
#include "mode.h"
#include "theme.h"

#include "utils/stringutils.h"

// A 'window' is like a vim window; i.e. a region inside a tab
namespace Zep
{

namespace
{
const uint32_t Color_CursorNormal = 0xEEF35FBC;
const uint32_t Color_CursorInsert = 0xFFFFFFFF;
}

ZepWindowBase::ZepWindowBase(ZepEditor& editor, ZepBuffer* buffer)
    : ZepComponent(editor),
    m_pBuffer(buffer)
{
}

ZepWindowBase::~ZepWindowBase()
{
}

NVec2i ZepWindowBase::BufferToDisplay()
{
    return BufferToDisplay(m_bufferCursor);
}

// TODO: this can be faster.
NVec2i ZepWindowBase::BufferToDisplay(const BufferLocation& loc)
{
    if (m_pendingLineUpdate)
    {
        UpdateVisibleLineData();
    }

    NVec2i ret(0, 0);
    if (windowLines.empty())
    {
        ret.x = 0;
        ret.y = 0;
        return ret;
    }

    // Only return a valid location if on the current display
    int lineCount = 0;
    for(auto& line : windowLines)  
    {
        if ((line.columnOffsets.x <= loc) &&
            (line.columnOffsets.y > loc))
        {
            ret.y = lineCount;
            ret.x = loc - line.columnOffsets.x;
            break;
        }
        lineCount++;
    }

    return ret;
}

void ZepWindowBase::MoveCursorInsideLine(LineLocation location)
{
    switch (location)
    {
    case LineLocation::LineBegin:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::BeyondLineEnd:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineCRBegin:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineFirstGraphChar:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineLastGraphChar:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    case LineLocation::LineLastNonCR:
        m_bufferCursor = m_pBuffer->GetLinePos(m_bufferCursor, location);
        break;
    }

    m_pendingLineUpdate = true;
}

void ZepWindowBase::MoveCursorTo(BufferLocation location)
{
    m_bufferCursor = m_pBuffer->Clamp(location);
    m_pendingLineUpdate = true;
   
    auto displayCursor = BufferToDisplay();
    auto& line = windowLines[displayCursor.y];
    lastCursorC = displayCursor.x;
}

void ZepWindowBase::MoveCursorWindowRelative(int yDistance, LineLocation clampLocation)
{
    auto cursorCL = BufferToDisplay();
    if (cursorCL.x == -1)
        return;

    // Find the screen line relative target
    auto target = cursorCL + NVec2i(0, yDistance);
    target.y = std::max(0l, target.y);
    target.y = std::min(target.y, long(windowLines.size() - 1));
    
    auto& line = windowLines[target.y];
   
    // Snap to the new vertical column if necessary (see comment below)
    if (target.x < lastCursorC)
        target.x = lastCursorC;

    // Update the master buffer cursor
    m_bufferCursor = line.columnOffsets.x + target.x;
  
    // Ensure the current x offset didn't walk us off the line (column offset is 1 beyond, and there is a single \n before it)
    // We are clamping to visible line here
    m_bufferCursor = std::min(m_bufferCursor, line.columnOffsets.y - 2);
    m_bufferCursor = std::max(m_bufferCursor, line.columnOffsets.x);

    GetEditor().ResetCursorTimer();
}

void ZepWindowBase::SetSelectionRange(BufferLocation start, BufferLocation end)
{
    selection.start = start;
    selection.end = end;
    selection.vertical = false;
    if (selection.start > selection.end)
    {
        std::swap(selection.start, selection.end);
    }
}

void ZepWindowBase::SetBuffer(ZepBuffer* pBuffer)
{
    assert(pBuffer);
    m_pBuffer = pBuffer;
    m_pendingLineUpdate = true;
}

BufferLocation ZepWindowBase::GetBufferCursor() const
{
    return m_bufferCursor;
}

ZepBuffer& ZepWindowBase::GetBuffer() const
{
    return *m_pBuffer;
}

} // Zep

