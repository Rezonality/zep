#pragma once

#include "buffer.h"

namespace Zep
{

class ZepTabWindow;

class ZepWindowBase : public ZepComponent
{
public:
    ZepWindowBase(ZepEditor& editor, ZepBuffer* buffer);
    virtual ~ZepWindowBase();

    // Cursor
    BufferLocation GetBufferCursor() const;
    void MoveCursorTo(BufferLocation location);
    void MoveCursorInsideLine(LineLocation location);
    void MoveCursorWindowRelative(int yDistance, LineLocation clampLocation = LineLocation::LineLastNonCR);

    NVec2i BufferToDisplay(const BufferLocation& location);
    NVec2i BufferToDisplay();

    void SetSelectionRange(BufferLocation start, BufferLocation end);

    ZepBuffer& GetBuffer() const;
    void SetBuffer(ZepBuffer* pBuffer);

protected:
    BufferLocation m_bufferCursor{ 0 }; // Location in buffer coordinates.  Each window has a different buffer cursor
    ZepBuffer* m_pBuffer = nullptr;
    long m_maxDisplayLines = 0;
    float m_defaultLineSize = 0;
    Region selection; // Selection area
    long lastCursorC = 0; // The last cursor column (could be removed and recalculated)
};

} // namespace Zep
