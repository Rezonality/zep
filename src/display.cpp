#include "display.h"
#include "syntax.h"
#include "buffer.h"

#include "utils/stringutils.h"
#include "utils/timer.h"

namespace Zep
{

namespace
{
const uint32_t Color_CursorNormal = 0xEEF35FBC;
const uint32_t Color_CursorInsert = 0xFFFFFFFF;
}

ZepDisplay::ZepDisplay(ZepEditor& editor)
    : ZepComponent(editor),
    m_spCursorTimer(new Timer())
{
}

ZepDisplay::~ZepDisplay()
{
}

void ZepDisplay::ResetCursorTimer()
{ 
    m_spCursorTimer->Restart(); 
}

void ZepDisplay::SetCurrentWindow(ZepWindow* pWindow)
{
    m_pCurrentWindow = pWindow;
    PreDisplay();
}

ZepWindow* ZepDisplay::GetCurrentWindow() const
{
    return m_pCurrentWindow;
}

ZepWindow* ZepDisplay::AddWindow()
{
    auto spWindow = std::make_shared<ZepWindow>(*this);
    m_windows.insert(spWindow);

    if (m_pCurrentWindow == nullptr)
    {
        m_pCurrentWindow = spWindow.get();
    }
    PreDisplay();
    return spWindow.get();
}

void ZepDisplay::RemoveWindow(ZepWindow* pWindow)
{
    for (auto& pWin : m_windows)
    {
        if (pWin.get() == pWindow)
        {
            if (m_pCurrentWindow == pWin.get())
            {
                m_pCurrentWindow = nullptr;
            }
            m_windows.erase(pWin);
            break;
        }
    }

    if (m_pCurrentWindow == nullptr)
    {
        if (!m_windows.empty())
        {
            m_pCurrentWindow = m_windows.begin()->get();
        }
    }
    PreDisplay();
}

const ZepDisplay::tWindows& ZepDisplay::GetWindows() const
{
    return m_windows;
}

void ZepDisplay::Notify(std::shared_ptr<ZepMessage> spMsg)
{
}

void ZepDisplay::SetDisplaySize(const NVec2f& topLeft, const NVec2f& bottomRight)
{
    m_topLeftPx = topLeft;
    m_bottomRightPx = bottomRight;
    PreDisplay();
}

void ZepDisplay::AssignDefaultWindow()
{
    if (m_pCurrentWindow != nullptr)
    {
        return;
    }

       
    auto pBuffer = GetEditor().GetMRUBuffer();
    assert(pBuffer);

    ZepWindow* pWindow = nullptr;
    if (!m_windows.empty())
    {
        pWindow = m_windows.begin()->get();
    }
    else
    {
        pWindow = AddWindow();
    }
    pWindow->SetCurrentBuffer(pBuffer);
}

// This function sets up the layout of everything on screen before drawing.
// It is easier to implement some things with up-front knowledge of where everything is on the screen
// and what each line contains
void ZepDisplay::PreDisplay()
{
    AssignDefaultWindow();

    auto commandCount = m_commandLines.size();
    const float commandSize = GetFontSize() * commandCount + textBorder * 2.0f;
    auto displaySize = m_bottomRightPx - m_topLeftPx;

    // Regions
    // TODO: A simple splitter manager to make calculating/moving these easier
    m_commandRegion.bottomRightPx = m_bottomRightPx;
    m_commandRegion.topLeftPx = m_commandRegion.bottomRightPx - NVec2f(displaySize.x, commandSize);

    auto current = m_topLeftPx;
    int i = 0;
    for (auto& win : m_windows)
    {
        auto topLeftPx = NVec2f((i * (displaySize.x / m_windows.size())) + m_topLeftPx.x, m_topLeftPx.y);
        auto bottomRightPx = NVec2f(topLeftPx.x + (displaySize.x / m_windows.size()), m_commandRegion.topLeftPx.y);
        win->PreDisplay(DisplayRegion{ topLeftPx, bottomRightPx });
        i++;
    }
}

struct LineData
{
    long lineStart = 0;
    long lineEnd = 0;
};

void ZepDisplay::Display()
{
    PreDisplay();

    // Always 1 command line
    if (m_commandLines.empty())
    {
        m_commandLines.push_back(" ");
    }
    long commandCount = long(m_commandLines.size());
    const float commandSize = GetFontSize() * commandCount + textBorder * 2.0f;

    auto displaySize = m_bottomRightPx - m_topLeftPx;

    auto commandSpace = commandCount;
    commandSpace = std::max(commandCount, 0l);

    // Background rect for status (airline)
    DrawRectFilled(m_commandRegion.topLeftPx, m_commandRegion.bottomRightPx, 0xFF111111);

    // Draw command text
    auto screenPosYPx = m_commandRegion.topLeftPx + NVec2f(0.0f, textBorder);
    for (int i = 0; i < commandSpace; i++)
    {
        auto textSize = GetTextSize((const utf8*)m_commandLines[i].c_str(),
            (const utf8*)m_commandLines[i].c_str() + m_commandLines[i].size());

        DrawChars(screenPosYPx,
            0xFFFFFFFF,
            (const utf8*)m_commandLines[i].c_str());

        screenPosYPx.y += GetFontSize();
        screenPosYPx.x = m_commandRegion.topLeftPx.x;
    }

    for (auto& win : m_windows)
    {
        win->Display();
    }
}

void ZepDisplay::SetCommandText(const std::string& strCommand)
{
    m_commandLines = StringUtils::SplitLines(strCommand);
    if (m_commandLines.empty())
    {
        m_commandLines.push_back("");
    }
}

void ZepDisplay::RequestRefresh()
{

}

bool ZepDisplay::RefreshRequired() const
{
    if (m_bPendingRefresh ||
        m_lastCursorBlink != GetCursorBlinkState())
    {
        m_bPendingRefresh = false;
        return true;
    }

    return false;
}

bool ZepDisplay::GetCursorBlinkState() const
{
    m_lastCursorBlink = (int(m_spCursorTimer->GetDelta() * 1.75f) & 1) ? true : false;
    return m_lastCursorBlink;
}

} // Zep