#include "editor.h"
#include "buffer.h"
#include "display.h"
#include "mode_standard.h"
#include "mode_vim.h"
#include "syntax.h"
#include "syntax_glsl.h"
#include "tab_window.h"
#include "utils/file.h"
#include "utils/stringutils.h"
#include "utils/timer.h"
#include "utils/logger.h"
#include "window.h"

structlog LOGCFG = { true, DEBUG };
namespace Zep
{

const char* Msg_Quit = "Quit";
const char* Msg_GetClipBoard = "GetClipBoard";
const char* Msg_SetClipBoard = "SetClipBoard";
const char* Msg_HandleCommand = "HandleCommand";

ZepComponent::ZepComponent(ZepEditor& editor)
    : m_editor(editor)
{
    m_editor.RegisterCallback(this);
}

ZepComponent::~ZepComponent()
{
    m_editor.UnRegisterCallback(this);
}

ZepEditor::ZepEditor(IZepDisplay* pDisplay, uint32_t flags)
    : m_flags(flags)
    , m_spCursorTimer(new Timer())
    , m_pDisplay(pDisplay)
{
    assert(m_pDisplay != nullptr);
    RegisterMode(VimMode, std::make_shared<ZepMode_Vim>(*this));
    RegisterMode(StandardMode, std::make_shared<ZepMode_Standard>(*this));
    SetMode(VimMode);

    RegisterSyntaxFactory("vert", tSyntaxFactory([](ZepBuffer* pBuffer) {
        return std::static_pointer_cast<ZepSyntax>(std::make_shared<ZepSyntaxGlsl>(*pBuffer));
    }));
}

ZepEditor::~ZepEditor()
{
    delete m_pDisplay;
}

void ZepEditor::SaveBuffer(ZepBuffer& buffer)
{
    // TODO:
    // - What if the buffer has no associated file?  Prompt for one.
    // - Report locked file situations, etc.
    // - We don't check for outside modification yet either, meaning this could overwrite
    std::ostringstream strText;
    int64_t size;
    if (!buffer.Save(size))
    {
        strText << "Failed to save: " << buffer.GetFilePath().string();
    }
    else
    {
        strText << "Wrote " << buffer.GetFilePath().string() << ", " << size << " bytes";
    }
    SetCommandText(strText.str());
}

void ZepEditor::InitWithFileOrDir(const std::string& str)
{
    fs::path startPath(str);

    // TODO
    if (fs::is_directory(startPath))
    {
    }
    else
    {
        ZepBuffer* pBuffer = AddBuffer(startPath.filename().string());
        pBuffer->Load(startPath);
    }
}

// At startup it's possible to be in a state where parts of the window framework are not yet in place.
// That's OK: the editor will just be blank.  But this isn't a 'normal' state, and the user shouldn't be able to close the last window, etc.
// without exiting the app - just like in Vim.
void ZepEditor::UpdateWindowState()
{
    // If there is no active tab window, and we have one, set it.
    if (!m_pActiveTabWindow)
    {
        if (!m_tabWindows.empty())
        {
            m_pActiveTabWindow = m_tabWindows.back();
        }
        else
        {
            m_pActiveTabWindow = AddTabWindow();
        }
    }

    // If the tab window doesn't contain an active window, and there is one, set it
    if (m_pActiveTabWindow && !m_pActiveTabWindow->GetActiveWindow())
    {
        if (!m_pActiveTabWindow->GetWindows().empty())
        {
            m_pActiveTabWindow->SetActiveWindow(m_pActiveTabWindow->GetWindows().back());
        }
        else
        {
            if (!m_buffers.empty())
            {
                m_pActiveTabWindow->AddWindow(m_buffers.back().get());
            }
            else
            {
                m_pActiveTabWindow->AddWindow(AddBuffer("[Empty]"));
            }
        }
    }

    if (m_pCurrentMode)
    {
        m_pCurrentMode->PreDisplay();
    }
}

void ZepEditor::ResetCursorTimer()
{
    m_spCursorTimer->Restart();
}

void ZepEditor::SetCurrentTabWindow(ZepTabWindow* pTabWindow)
{
    m_pActiveTabWindow = pTabWindow;
}

ZepTabWindow* ZepEditor::GetActiveTabWindow() const
{
    return m_pActiveTabWindow;
}

ZepTabWindow* ZepEditor::AddTabWindow()
{
    auto pTabWindow = new ZepTabWindow(*this);
    m_tabWindows.push_back(pTabWindow);

    if (m_pActiveTabWindow == nullptr)
    {
        m_pActiveTabWindow = pTabWindow;
    }

    return pTabWindow;
}

void ZepEditor::Quit()
{
    Broadcast(std::make_shared<ZepMessage>(Msg_Quit, "Quit"));
}

void ZepEditor::RemoveTabWindow(ZepTabWindow* pTabWindow)
{
    assert(pTabWindow);
    if (!pTabWindow)
        return;

    auto itrFound = std::find(m_tabWindows.begin(), m_tabWindows.end(), pTabWindow);
    if (itrFound == m_tabWindows.end())
    {
        assert(!"Not found?");
        return;
    }

    delete pTabWindow;
    m_tabWindows.erase(itrFound);

    if (m_tabWindows.empty())
    {
        m_pActiveTabWindow = nullptr;
        Quit();
    }
    else
    {
        if (m_pActiveTabWindow == pTabWindow)
        {
            m_pActiveTabWindow = m_tabWindows[m_tabWindows.size() - 1];
        }
    }
}

const ZepEditor::tTabWindows& ZepEditor::GetTabWindows() const
{
    return m_tabWindows;
}

void ZepEditor::RegisterMode(const std::string& name, std::shared_ptr<ZepMode> spMode)
{
    m_mapModes[name] = spMode;
}

void ZepEditor::SetMode(const std::string& mode)
{
    auto itrMode = m_mapModes.find(mode);
    if (itrMode != m_mapModes.end())
    {
        m_pCurrentMode = itrMode->second.get();
        m_pCurrentMode->Begin();
    }
}

ZepMode* ZepEditor::GetCurrentMode()
{
    if (!m_pCurrentMode && !m_mapModes.empty())
    {
        m_pCurrentMode = m_mapModes.begin()->second.get();
    }

    return m_pCurrentMode;
}

void ZepEditor::RegisterSyntaxFactory(const std::string& extension, tSyntaxFactory factory)
{
    m_mapSyntax[extension] = factory;
}

// Inform clients of an event in the buffer
bool ZepEditor::Broadcast(std::shared_ptr<ZepMessage> message)
{
    // Better place for this?
    ResetCursorTimer();

    Notify(message);
    if (message->handled)
        return true;

    for (auto& client : m_notifyClients)
    {
        client->Notify(message);
        if (message->handled)
            break;
    }
    return message->handled;
}

const std::deque<std::shared_ptr<ZepBuffer>>& ZepEditor::GetBuffers() const
{
    return m_buffers;
}

ZepBuffer* ZepEditor::AddBuffer(const std::string& str)
{
    auto pBuffer = std::make_shared<ZepBuffer>(*this, str);
    m_buffers.push_front(pBuffer);

    if (auto extOffset = str.find_last_of('.'); extOffset != std::string::npos)
    {
        auto itrFactory = m_mapSyntax.find(str.substr(extOffset + 1, str.size() - extOffset));
        if (itrFactory != m_mapSyntax.end())
        {
            pBuffer->SetSyntax(itrFactory->second(pBuffer.get()));
        }
        else
        {
            // For now, the first syntax file is the default
            if (!m_mapSyntax.empty())
            {
                pBuffer->SetSyntax(m_mapSyntax.begin()->second(pBuffer.get()));
            }
        }
    }

    // Adding a buffer immediately updates the window state, in case this is the first one
    UpdateWindowState();

    return pBuffer.get();
}

ZepBuffer* ZepEditor::GetMRUBuffer() const
{
    return m_buffers.front().get();
}

void ZepEditor::SetRegister(const std::string& reg, const Register& val)
{
    m_registers[reg] = val;
}

void ZepEditor::SetRegister(const char reg, const Register& val)
{
    std::string str({ reg });
    m_registers[str] = val;
}

void ZepEditor::SetRegister(const std::string& reg, const char* pszText)
{
    m_registers[reg] = Register(pszText);
}

void ZepEditor::SetRegister(const char reg, const char* pszText)
{
    std::string str({ reg });
    m_registers[str] = Register(pszText);
}

Register& ZepEditor::GetRegister(const std::string& reg)
{
    return m_registers[reg];
}

Register& ZepEditor::GetRegister(const char reg)
{
    std::string str({ reg });
    return m_registers[str];
}
const tRegisters& ZepEditor::GetRegisters() const
{
    return m_registers;
}

void ZepEditor::Notify(std::shared_ptr<ZepMessage> message)
{
}

void ZepEditor::SetCommandText(const std::string& strCommand)
{
    m_commandLines = StringUtils::SplitLines(strCommand);
    if (m_commandLines.empty())
    {
        m_commandLines.push_back("");
    }
    m_bRegionsChanged = true;
}

void ZepEditor::RequestRefresh()
{
}

bool ZepEditor::RefreshRequired() const
{
    if (m_bPendingRefresh || m_lastCursorBlink != GetCursorBlinkState())
    {
        m_bPendingRefresh = false;
        return true;
    }

    return false;
}

bool ZepEditor::GetCursorBlinkState() const
{
    m_lastCursorBlink = (int(m_spCursorTimer->GetDelta() * 1.75f) & 1) ? true : false;
    return m_lastCursorBlink;
}

void ZepEditor::SetDisplayRegion(const NVec2f& topLeft, const NVec2f& bottomRight)
{
    m_topLeftPx = topLeft;
    m_bottomRightPx = bottomRight;
    UpdateSize();
}

void ZepEditor::UpdateSize()
{
    auto commandCount = GetCommandLines().size();
    const float commandSize = m_pDisplay->GetFontSize() * commandCount + textBorder * 2.0f;
    auto displaySize = m_bottomRightPx - m_topLeftPx;

    // Regions
    // TODO: A simple splitter manager to make calculating/moving these easier
    m_commandRegion.bottomRightPx = m_bottomRightPx;
    m_commandRegion.topLeftPx = m_commandRegion.bottomRightPx - NVec2f(displaySize.x, commandSize);

    // Add tabs for extra windows
    if (GetTabWindows().size() > 1)
    {
        m_tabRegion.topLeftPx = m_topLeftPx;
        m_tabRegion.bottomRightPx = m_tabRegion.topLeftPx + NVec2f(displaySize.x, m_pDisplay->GetFontSize() + textBorder * 2);
    }
    else
    {
        // Empty
        m_tabRegion.topLeftPx = m_topLeftPx;
        m_tabRegion.bottomRightPx = m_topLeftPx + NVec2f(displaySize.x, 0.0f);
    }

    m_tabContentRegion.topLeftPx = m_tabRegion.bottomRightPx + NVec2f(-displaySize.x, 1.0f);
    m_tabContentRegion.bottomRightPx = m_commandRegion.topLeftPx + NVec2f(displaySize.x, 0.0f);

    GetActiveTabWindow()->SetDisplayRegion(m_tabContentRegion);
}

void ZepEditor::Display()
{
    if (m_bRegionsChanged)
    {
        m_bRegionsChanged = false;
        UpdateSize();
    }
    // Ensure window state is good ??
    UpdateWindowState();

    // Always 1 command line
    auto& commandLines = GetCommandLines();

    long commandCount = long(commandLines.size());
    const float commandSize = m_pDisplay->GetFontSize() * commandCount + textBorder * 2.0f;

    auto displaySize = m_bottomRightPx - m_topLeftPx;

    auto commandSpace = commandCount;
    commandSpace = std::max(commandCount, 0l);

    // Background rect for status (airline)
    m_pDisplay->DrawRectFilled(m_commandRegion.topLeftPx, m_commandRegion.bottomRightPx, 0xFF333333);

    // Draw command text
    auto screenPosYPx = m_commandRegion.topLeftPx + NVec2f(0.0f, textBorder);
    for (int i = 0; i < commandSpace; i++)
    {
        auto textSize = m_pDisplay->GetTextSize((const utf8*)commandLines[i].c_str(),
            (const utf8*)commandLines[i].c_str() + commandLines[i].size());

        m_pDisplay->DrawChars(screenPosYPx,
            0xFFFFFFFF,
            (const utf8*)commandLines[i].c_str());

        screenPosYPx.y += m_pDisplay->GetFontSize();
        screenPosYPx.x = m_commandRegion.topLeftPx.x;
    }

    if (GetTabWindows().size() > 1)
    {
        // Tab region
        // TODO Handle it when tabs are bigger than the available width!
        m_pDisplay->DrawRectFilled(m_tabRegion.BottomLeft() - NVec2f(0.0f, 2.0f), m_tabRegion.bottomRightPx, 0xFF888888);
        NVec2f currentTab = m_tabRegion.topLeftPx;
        for (auto& window : GetTabWindows())
        {
            // Show active buffer in tab as tab name
            auto& buffer = window->GetActiveWindow()->GetBuffer();
            auto tabColor = (window == GetActiveTabWindow()) ? 0xFF666666 : 0x44888888;
            auto tabLength = m_pDisplay->GetTextSize((utf8*)buffer.GetName().c_str()).x + textBorder * 2;
            m_pDisplay->DrawRectFilled(currentTab, currentTab + NVec2f(tabLength, m_tabRegion.Height()), tabColor);

            m_pDisplay->DrawChars(currentTab + NVec2f(textBorder, textBorder), 0xFFFFFFFF, (utf8*)buffer.GetName().c_str());

            currentTab.x += tabLength + textBorder;
        }
    }

    // Display the tab
    if (GetActiveTabWindow())
    {
        GetActiveTabWindow()->Display();
    }
}

} // namespace Zep
