#include "display.h"
#include "syntax.h"
#include "buffer.h"
#include "window.h"
#include "tab_window.h"
#include "editor.h"

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
    : ZepComponent(editor)
{
}

ZepDisplay::~ZepDisplay()
{
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

// This function sets up the layout of everything on screen before drawing.
// It is easier to implement some things with up-front knowledge of where everything is on the screen
// and what each line contains
void ZepDisplay::PreDisplay()
{
    auto commandCount = GetEditor().GetCommandLines().size();
    const float commandSize = GetFontSize() * commandCount + textBorder * 2.0f;
    auto displaySize = m_bottomRightPx - m_topLeftPx;

    // Regions
    // TODO: A simple splitter manager to make calculating/moving these easier
    m_commandRegion.bottomRightPx = m_bottomRightPx;
    m_commandRegion.topLeftPx = m_commandRegion.bottomRightPx - NVec2f(displaySize.x, commandSize);

    // Add tabs for extra windows
    if (GetEditor().GetTabWindows().size() > 1)
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

    if (GetEditor().GetActiveTabWindow())
    {
        GetEditor().GetActiveTabWindow()->PreDisplay(*this, m_tabContentRegion);
    }
}

struct LineData
{
    long lineStart = 0;
    long lineEnd = 0;
};

void ZepDisplay::Display()
{
    // Ensure window state is good
    GetEditor().UpdateWindowState();

    PreDisplay();

    // Always 1 command line
    auto& commandLines = GetEditor().GetCommandLines();

    long commandCount = long(commandLines.size());
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
        auto textSize = GetTextSize((const utf8*)commandLines[i].c_str(),
            (const utf8*)commandLines[i].c_str() + commandLines[i].size());

        DrawChars(screenPosYPx,
            0xFFFFFFFF,
            (const utf8*)commandLines[i].c_str());

        screenPosYPx.y += GetFontSize();
        screenPosYPx.x = m_commandRegion.topLeftPx.x;
    }
 
    if (GetEditor().GetTabWindows().size() > 1)
    {
        // Tab region 
        // TODO Handle it when tabs are bigger than the available width!
        DrawRectFilled(m_tabRegion.BottomLeft() - NVec2f(0.0f, 2.0f), m_tabRegion.bottomRightPx, 0xFF888888);
        NVec2f currentTab = m_tabRegion.topLeftPx;
        for (auto& window : GetEditor().GetTabWindows())
        {
            // Show active buffer in tab as tab name
            auto& buffer = window->GetActiveWindow()->GetBuffer();
            auto tabColor = (window == GetEditor().GetActiveTabWindow()) ? 0xFF666666 : 0x44888888;
            auto tabLength = GetTextSize((utf8*)buffer.GetName().c_str()).x + textBorder * 2;
            DrawRectFilled(currentTab, currentTab + NVec2f(tabLength, m_tabRegion.Height()), tabColor);

            DrawChars(currentTab + NVec2f(textBorder, textBorder), 0xFFFFFFFF, (utf8*)buffer.GetName().c_str());

            currentTab.x += tabLength + textBorder;
        }
    }

    // Display the tab
    if (GetEditor().GetActiveTabWindow())
    {
        GetEditor().GetActiveTabWindow()->Display(*this);
    }
}

} // Zep