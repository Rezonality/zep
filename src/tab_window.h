#pragma once

#include "buffer.h"
#include <deque>

namespace Zep
{

class ZepDisplay;
class ZepWindow;

// Display state for a single pane of text.
// Editor operations such as select and change are local to a displayed pane
class ZepTabWindow : public ZepComponent
{
public:

    ZepTabWindow(ZepEditor& editor);
    virtual ~ZepTabWindow();

    virtual void Notify(std::shared_ptr<ZepMessage> message) override;


    ZepWindow* AddWindow(ZepBuffer* pBuffer);
    void RemoveWindow(ZepWindow* pWindow);
    void SetActiveWindow(ZepWindow* pBuffer) { m_pActiveWindow = pBuffer; }
    ZepWindow* GetActiveWindow() const { return m_pActiveWindow; }
    void CloseActiveWindow();
    
    using tWindows = std::vector<ZepWindow*>;
    const tWindows& GetWindows() const { return m_windows; }

    void PreDisplay(ZepDisplay& display, const DisplayRegion& region);
    void Display(ZepDisplay& display);

private:
    ZepEditor& m_editor;                            // Editor that owns this window
    DisplayRegion m_windowRegion;                 // region of the display we are showing on.
    DisplayRegion m_buffersRegion;                   // region of the display for text.

    tWindows m_windows;
    ZepWindow* m_pActiveWindow = nullptr;
};

} // Zep
