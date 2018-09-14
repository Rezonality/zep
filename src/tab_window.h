#pragma once

#include "buffer.h"

namespace Zep
{

class ZepDisplay;
class ZepWindow;

// Display state for a single pane of text.
// Editor operations such as select and change are local to a displayed pane
class ZepTabWindow : public ZepComponent
{
public:
    using tBuffers = std::map<ZepBuffer*, std::shared_ptr<ZepWindow>>;

    ZepTabWindow(ZepDisplay& display);
    virtual ~ZepTabWindow();

    virtual void Notify(std::shared_ptr<ZepMessage> message) override;

    void PreDisplay(const DisplayRegion& region);

    ZepWindow* GetCurrentWindow() const;

    void SetCurrentBuffer(ZepBuffer* pBuffer);
    ZepBuffer* GetCurrentBuffer() const;
    void AddBuffer(ZepBuffer* pBuffer);
    void RemoveBuffer(ZepBuffer* pBuffer);
    const tBuffers& GetBuffers() const;

    ZepDisplay& GetDisplay() const { return m_display; }
    void Display();

private:
    ZepDisplay& m_display;                     // Display that owns this window
    DisplayRegion m_windowRegion;                 // region of the display we are showing on.
    DisplayRegion m_buffersRegion;                   // region of the display for text.

    tBuffers m_buffers;
    ZepBuffer* m_pCurrentBuffer = nullptr;
};

} // Zep
