#include "display.h"
#include "syntax.h"
#include "buffer.h"
#include "tab_window.h"

#include "utils/stringutils.h"
#include "utils/timer.h"

namespace Zep
{

namespace
{
const uint32_t Color_CursorNormal = 0xEEF35FBC;
const uint32_t Color_CursorInsert = 0xFFFFFFFF;
//const float TabSize = 20.0f;
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

void ZepDisplay::SetCurrentWindow(ZepTabWindow* pTabWindow)
{
    m_pCurrentTabWindow = pTabWindow;
    PreDisplay();
}

ZepTabWindow* ZepDisplay::GetCurrentWindow() const
{
    return m_pCurrentTabWindow;
}

ZepTabWindow* ZepDisplay::AddWindow()
{
    auto spWindow = std::make_shared<ZepTabWindow>(*this);
    m_windows.insert(spWindow);

    if (m_pCurrentTabWindow == nullptr)
    {
        m_pCurrentTabWindow = spWindow.get();
    }
    PreDisplay();
    return spWindow.get();
}

void ZepDisplay::RemoveWindow(ZepTabWindow* pTabWindow)
{
    for (auto& pWin : m_windows)
    {
        if (pWin.get() == pTabWindow)
        {
            if (m_pCurrentTabWindow == pWin.get())
            {
                m_pCurrentTabWindow = nullptr;
            }
            m_windows.erase(pWin);
            break;
        }
    }

    if (m_pCurrentTabWindow == nullptr)
    {
        if (!m_windows.empty())
        {
            m_pCurrentTabWindow = m_windows.begin()->get();
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
    if (m_pCurrentTabWindow != nullptr)
    {
        return;
    }

    auto pBuffer = GetEditor().GetMRUBuffer();
    assert(pBuffer);

    ZepTabWindow* pTabWindow = nullptr;
    if (!m_windows.empty())
    {
        pTabWindow = m_windows.begin()->get();
    }
    else
    {
        pTabWindow = AddWindow();
    }
    pTabWindow->SetCurrentBuffer(pBuffer);
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

    // Add tabs for extra windows
    if (m_windows.size() > 1)
    {
        m_tabRegion.topLeftPx = m_topLeftPx;
        m_tabRegion.bottomRightPx = m_tabRegion.topLeftPx + NVec2f(displaySize.x, GetFontSize() + textBorder * 2);
    }
    else
    {
        // Empty
        m_tabRegion.topLeftPx = m_topLeftPx;
        m_tabRegion.bottomRightPx = m_topLeftPx + NVec2f(displaySize.x, 0.0f);
    }

    m_tabContentRegion.topLeftPx = m_tabRegion.bottomRightPx + NVec2f(-displaySize.x, 0.0f);
    m_tabContentRegion.bottomRightPx = m_commandRegion.topLeftPx + NVec2f(displaySize.x, 0.0f);

    if (m_pCurrentTabWindow)
    {
        m_pCurrentTabWindow->PreDisplay(m_tabContentRegion);
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
    DrawRectFilled(m_commandRegion.topLeftPx, m_commandRegion.bottomRightPx, 0xFF333333);

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
 
    if (m_windows.size() > 1)
    {
        // Tab region 
        // TODO Handle it when tabs are bigger than the available width!
        DrawRectFilled(m_tabRegion.BottomLeft() - NVec2f(0.0f, 2.0f), m_tabRegion.bottomRightPx, 0xFF888888);
        NVec2f currentTab = m_tabRegion.topLeftPx;
        for (auto& window : m_windows)
        {
            // Show active buffer in tab as tab name
            auto pBuffer = window->GetCurrentBuffer();
            auto tabColor = (window.get() == m_pCurrentTabWindow) ? 0xFF666666 : 0x44888888;
            auto tabLength = GetTextSize((utf8*)pBuffer->GetName().c_str()).x + textBorder * 2;
            DrawRectFilled(currentTab, currentTab + NVec2f(tabLength, m_tabRegion.Height()), tabColor);

            DrawChars(currentTab + NVec2f(textBorder, textBorder), 0xFFFFFFFF, (utf8*)pBuffer->GetName().c_str());

            currentTab.x += tabLength + textBorder;
        }
    }

    // Display the tab
    if (m_pCurrentTabWindow)
    {
        m_pCurrentTabWindow->Display();
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