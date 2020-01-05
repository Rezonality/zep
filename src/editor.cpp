#include "zep/editor.h"
#include "zep/buffer.h"
#include "zep/display.h"
#include "zep/filesystem.h"
#include "zep/mode_repl.h"
#include "zep/mode_tree.h"
#include "zep/mode_search.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/syntax.h"
#include "zep/syntax_providers.h"
#include "zep/tab_window.h"
#include "zep/theme.h"
#include "zep/window.h"

#include "zep/mcommon/animation/timer.h"
#include "zep/mcommon/file/cpptoml.h"
#include "zep/mcommon/file/path.h"
#include "zep/mcommon/logger.h"
#include "zep/mcommon/string/murmur_hash.h"
#include "zep/mcommon/string/stringutils.h"

#include <stdexcept>

namespace Zep
{
structlog LOGCFG = { true, DEBUG };
}

namespace Zep
{

ZepComponent::ZepComponent(ZepEditor& editor)
    : m_editor(editor)
{
    m_editor.RegisterCallback(this);
}

ZepComponent::~ZepComponent()
{
    m_editor.UnRegisterCallback(this);
}

ZepEditor::ZepEditor(ZepDisplay* pDisplay, const ZepPath& root, uint32_t flags, IZepFileSystem* pFileSystem)
    : m_pDisplay(pDisplay)
    , m_pFileSystem(pFileSystem)
    , m_flags(flags)
    , m_rootPath(root)
{

#if defined(ZEP_FEATURE_CPP_FILE_SYSTEM)
    if (m_pFileSystem == nullptr)
    {
        m_pFileSystem = new ZepFileSystemCPP();
    }
#else
    if (m_pFileSystem == nullptr)
    {
        assert(!"Must supply a file system - no default available on this platform!");
        throw std::invalid_argument("pFileSystem");
    }
#endif

    if (m_flags & ZepEditorFlags::DisableThreads)
    {
        m_threadPool = std::make_unique<ThreadPool>(1);
    }
    else
    {
        m_threadPool = std::make_unique<ThreadPool>();
    }

    LoadConfig(root / "zep.cfg");

    m_spTheme = std::make_shared<ZepTheme>();

    assert(m_pDisplay != nullptr);
    RegisterGlobalMode(std::make_shared<ZepMode_Vim>(*this));
    RegisterGlobalMode(std::make_shared<ZepMode_Standard>(*this));
    SetGlobalMode(ZepMode_Vim::StaticName());

    timer_restart(m_cursorTimer);
    timer_restart(m_lastEditTimer);
    m_commandLines.push_back("");

    RegisterSyntaxProviders(*this);

    m_editorRegion = std::make_shared<Region>();
    m_editorRegion->vertical = false;

    m_tabRegion = std::make_shared<Region>();
    m_tabContentRegion = std::make_shared<Region>();
    m_commandRegion = std::make_shared<Region>();

    m_editorRegion->children.push_back(m_tabRegion);
    m_editorRegion->children.push_back(m_tabContentRegion);
    m_editorRegion->children.push_back(m_commandRegion);

    Reset();
}

ZepEditor::~ZepEditor()
{
    delete m_pDisplay;
    delete m_pFileSystem;
}

ThreadPool& ZepEditor::GetThreadPool() const
{
    return *m_threadPool;
}

void ZepEditor::OnFileChanged(const ZepPath& path)
{
    if (path.filename() == "zep.cfg")
    {
        LOG(INFO) << "Reloading config";
        LoadConfig(path);
        Broadcast(std::make_shared<ZepMessage>(Msg::ConfigChanged));
    }
}

// If you pass a valid path to a 'zep.cfg' file, then editor settings will serialize from that
// You can even edit it inside zep for immediate changes :)
void ZepEditor::LoadConfig(const ZepPath& config_path)
{
    if (!GetFileSystem().Exists(config_path))
    {
        return;
    }

    try
    {
        std::shared_ptr<cpptoml::table> spConfig;
        spConfig = cpptoml::parse_file(config_path.string());
        if (spConfig == nullptr)
            return;

        LoadConfig(spConfig);
    }
    catch (cpptoml::parse_exception& ex)
    {
        std::ostringstream str;
        str << config_path.filename().string() << " : Failed to parse. " << ex.what();
        SetCommandText(str.str());
    }
    catch (...)
    {
        std::ostringstream str;
        str << config_path.filename().string() << " : Failed to parse. ";
        SetCommandText(str.str());
    }
}

void ZepEditor::LoadConfig(std::shared_ptr<cpptoml::table> spConfig)
{
    try
    {
        m_config.showIndicatorRegion = spConfig->get_qualified_as<bool>("editor.show_indicator_region").value_or(true);
        m_config.showLineNumbers = spConfig->get_qualified_as<bool>("editor.show_line_numbers").value_or(true);
        m_config.autoHideCommandRegion = spConfig->get_qualified_as<bool>("editor.autohide_command_region").value_or(false);
        m_config.cursorLineSolid = spConfig->get_qualified_as<bool>("editor.cursor_line_solid").value_or(true);
        m_config.backgroundFadeTime = (float)spConfig->get_qualified_as<double>("editor.background_fade_time").value_or(60.0f);
        m_config.backgroundFadeWait = (float)spConfig->get_qualified_as<double>("editor.background_fade_wait").value_or(60.0f);
        m_config.showScrollBar = spConfig->get_qualified_as<uint32_t>("editor.show_scrollbar").value_or(1);
        m_config.lineMargins.x = (float)spConfig->get_qualified_as<double>("editor.line_margin_top").value_or(1);
        m_config.lineMargins.y = (float)spConfig->get_qualified_as<double>("editor.line_margin_bottom").value_or(1);
        m_config.widgetMargins.x = (float)spConfig->get_qualified_as<double>("editor.widget_margin_top").value_or(1);
        m_config.widgetMargins.y = (float)spConfig->get_qualified_as<double>("editor.widget_margin_bottom").value_or(1);
        m_config.shortTabNames = spConfig->get_qualified_as<bool>("editor.short_tab_names").value_or(false);
        auto styleStr = string_tolower(spConfig->get_qualified_as<std::string>("editor.style").value_or("normal"));
        if (styleStr == "normal")
        {
            m_config.style = EditorStyle::Normal;
        }
        else if (styleStr == "minimal")
        {
            m_config.style = EditorStyle::Minimal;
        }
    }
    catch (...)
    {
    }
}

void ZepEditor::SaveConfig(std::shared_ptr<cpptoml::table> spConfig)
{
    auto table = spConfig->get_table("editor");
    if (!table)
    {
        table = cpptoml::make_table();
        spConfig->insert("editor", table);
    }

    table->insert("show_indicator_region", m_config.showIndicatorRegion);
    table->insert("show_line_numbers", m_config.showLineNumbers);
    table->insert("autohide_command_region", m_config.autoHideCommandRegion);
    table->insert("cursor_line_solid", m_config.cursorLineSolid);
    table->insert("short_tab_names", m_config.shortTabNames);
    table->insert("background_fade_time", (double)m_config.backgroundFadeTime);
    table->insert("background_fade_wait", (double)m_config.backgroundFadeWait);
    table->insert("show_scrollbar", m_config.showScrollBar);

    table->insert("line_margin_top", m_config.lineMargins.x);
    table->insert("line_margin_bottom", m_config.lineMargins.y);
    table->insert("widget_margin_top", m_config.widgetMargins.x);
    table->insert("widget_margin_bottom", m_config.widgetMargins.y);

    table->insert("style", m_config.style == EditorStyle::Minimal ? "minimal" : "normal");

    /*
    Example Write:
    std::ofstream stream("d:/dev/out.txt");
    cpptoml::toml_writer writer(stream, "");
    writer.visit(*spConfig);
    */
}

void ZepEditor::SaveBuffer(ZepBuffer& buffer)
{
    // TODO:
    // - What if the buffer has no associated file?  Prompt for one.
    // - We don't check for outside modification yet either, meaning this could overwrite
    std::ostringstream strText;

    if (buffer.TestFlags(FileFlags::ReadOnly))
    {
        strText << "Failed to save, Read Only: " << buffer.GetDisplayName();
    }
    else if (buffer.TestFlags(FileFlags::Locked))
    {
        strText << "Failed to save, Locked: " << buffer.GetDisplayName();
    }
    else if (buffer.GetFilePath().empty())
    {
        strText << "Error: No file name";
    }
    else
    {
        int64_t size;
        if (!buffer.Save(size))
        {
            strText << "Failed to save: " << buffer.GetDisplayName() << " at: " << buffer.GetFilePath().string();
        }
        else
        {
            strText << "Wrote " << buffer.GetFilePath().string() << ", " << size << " bytes";
        }
    }
    SetCommandText(strText.str());
}

std::vector<ZepWindow*> ZepEditor::FindBufferWindows(const ZepBuffer* pBuffer) const
{
    std::vector<ZepWindow*> bufferWindows;
    for (auto& tab : m_tabWindows)
    {
        for (auto& win : tab->GetWindows())
        {
            if (&win->GetBuffer() == pBuffer)
            {
                bufferWindows.push_back(win);
            }
        }
    }
    return bufferWindows;
}

void ZepEditor::RemoveBuffer(ZepBuffer* pBuffer)
{
    auto bufferWindows = FindBufferWindows(pBuffer);
    for (auto& window : bufferWindows)
    {
        window->GetTabWindow().RemoveWindow(window);
    }

    // Find the buffer in the list of buffers owned by the editor and remove it
    auto itr = std::find_if(m_buffers.begin(), m_buffers.end(), [pBuffer](std::shared_ptr<ZepBuffer> spBuffer) {
        return spBuffer.get() == pBuffer;
    });

    if (itr != m_buffers.end())
    {
        m_buffers.erase(itr);
    }
}

ZepBuffer* ZepEditor::GetEmptyBuffer(const std::string& name, uint32_t fileFlags)
{
    auto pBuffer = CreateNewBuffer(name);
    pBuffer->SetFlags(fileFlags, true);
    return pBuffer;
}

ZepBuffer* ZepEditor::GetFileBuffer(const ZepPath& filePath, uint32_t fileFlags, bool create)
{
    auto path = GetFileSystem().Exists(filePath) ? GetFileSystem().Canonical(filePath) : filePath;
    if (!path.empty())
    {
        for (auto& pBuffer : m_buffers)
        {
            if (!pBuffer->GetFilePath().empty())
            {
                if (GetFileSystem().Equivalent(pBuffer->GetFilePath(), path))
                {
                    //LOG(DEBUG) << "Found equivalent buffer for file: " << path.string();
                    return pBuffer.get();
                }
            }
        }
    }

    if (!create)
    {
        return nullptr;
    }

    // Create buffer, try to load even if not present, the buffer represents the save path (it just isn't saved yet)
    auto pBuffer = CreateNewBuffer(filePath);

    pBuffer->SetFlags(fileFlags, true);
    return pBuffer;
}

// TODO: Cleaner handling of window/mode/modal stuff.
ZepWindow* ZepEditor::AddRepl()
{
    if (!GetActiveTabWindow())
    {
        return nullptr;
    }

    auto pActiveWindow = GetActiveTabWindow()->GetActiveWindow();

    auto pReplBuffer = GetEmptyBuffer("Repl.lisp", FileFlags::Locked);
    pReplBuffer->SetBufferType(BufferType::Repl);

    auto pReplWindow = GetActiveTabWindow()->AddWindow(pReplBuffer, nullptr, false);

    auto pMode = std::make_shared<ZepMode_Repl>(*this, *pActiveWindow, *pReplWindow);
    pReplBuffer->SetMode(pMode);
    pMode->Begin();
    return pReplWindow;
}

ZepWindow* ZepEditor::AddOrca()
{
    auto pOrcaBuffer = GetEmptyBuffer("Orca.orca");
    GetActiveTabWindow()->GetActiveWindow()->SetBuffer(pOrcaBuffer);
    return GetActiveTabWindow()->GetActiveWindow();
}

ZepWindow* ZepEditor::AddTree()
{
    auto pTree = GetEmptyBuffer("Tree.tree");
    auto pTreeWindow = GetActiveTabWindow()->AddWindow(pTree, nullptr, true);

    auto pActiveWindow = GetActiveTabWindow()->GetActiveWindow();
    pActiveWindow->SetBuffer(pTree);
    
    auto pMode = std::make_shared<ZepMode_Tree>(*this, *pActiveWindow, *pTreeWindow);
    pTree->SetMode(pMode);
    pMode->Begin();
    return pActiveWindow;
}

ZepWindow* ZepEditor::AddSearch()
{
    if (!GetActiveTabWindow())
    {
        return nullptr;
    }

    auto pSearchBuffer = GetEmptyBuffer("Search", FileFlags::Locked | FileFlags::ReadOnly);
    pSearchBuffer->SetBufferType(BufferType::Search);

    auto pActiveWindow = GetActiveTabWindow()->GetActiveWindow();
    auto searchPath = GetFileSystem().GetSearchRoot(pActiveWindow->GetBuffer().GetFilePath());

    auto pSearchWindow = GetActiveTabWindow()->AddWindow(pSearchBuffer, nullptr, false);
    pSearchWindow->SetWindowFlags(pSearchWindow->GetWindowFlags() & WindowFlags::Modal);
    pSearchWindow->SetCursorType(CursorType::LineMarker);

    auto pMode = std::make_shared<ZepMode_Search>(*this, *pActiveWindow, *pSearchWindow, searchPath);
    pSearchBuffer->SetMode(pMode);
    pMode->Begin();
    return pSearchWindow;
}

ZepTabWindow* ZepEditor::EnsureTab()
{
    if (m_tabWindows.empty())
    {
        return AddTabWindow();
    }

    if (m_pActiveTabWindow)
    {
        return m_pActiveTabWindow;
    }
    return m_tabWindows[0];
}

// Reset editor to start state; with a single tab, a single window and an empty unmodified buffer
void ZepEditor::Reset()
{
    EnsureTab();
}

// TODO fix for directory startup; it won't work
ZepBuffer* ZepEditor::InitWithFileOrDir(const std::string& str)
{
    ZepPath startPath(str);

    if (GetFileSystem().Exists(startPath))
    {
        startPath = GetFileSystem().Canonical(startPath);
    }

    // If a directory, just return the default already created buffer.
    if (GetFileSystem().IsDirectory(startPath))
    {
        GetFileSystem().SetWorkingDirectory(startPath);
        return &GetActiveTabWindow()->GetActiveWindow()->GetBuffer();
    }

    // Get a buffer for the start file
    auto pFileBuffer = GetFileBuffer(startPath);
    auto pTab = EnsureTab();
    pTab->AddWindow(pFileBuffer, nullptr, false);

    return pFileBuffer;
}

ZepBuffer* ZepEditor::InitWithText(const std::string& strName, const std::string& strText)
{
    auto pTab = EnsureTab();

    auto pBuffer = GetEmptyBuffer(strName);
    pBuffer->SetText(strText);

    pTab->AddWindow(pBuffer, nullptr, false);

    return pBuffer;
}

// Here we ensure that the editor is in a valid state, and cleanup Default buffers
void ZepEditor::UpdateWindowState()
{
    // If there is no active tab window, and we have one, set it.
    if (!m_pActiveTabWindow)
    {
        if (!m_tabWindows.empty())
        {
            m_pActiveTabWindow = m_tabWindows.back();
        }
    }

    // If the tab window doesn't contain an active window, and there is one, set it
    if (m_pActiveTabWindow && !m_pActiveTabWindow->GetActiveWindow())
    {
        if (!m_pActiveTabWindow->GetWindows().empty())
        {
            m_pActiveTabWindow->SetActiveWindow(m_pActiveTabWindow->GetWindows().back());
            m_bRegionsChanged = true;
        }
    }

    // Clean up any default buffers
    std::vector<ZepBuffer*> victims;
    for (auto& buffer : m_buffers)
    {
        if (!buffer->TestFlags(FileFlags::DefaultBuffer) || buffer->TestFlags(FileFlags::Dirty))
        {
            continue;
        }

        auto windows = FindBufferWindows(buffer.get());
        if (windows.empty())
        {
            victims.push_back(buffer.get());
        }
    }

    for (auto& victim : victims)
    {
        RemoveBuffer(victim);
    }

    if (m_pCurrentMode)
    {
        m_pCurrentMode->PreDisplay();
    }
}

void ZepEditor::ResetCursorTimer()
{
    timer_restart(m_cursorTimer);
}

void ZepEditor::ResetLastEditTimer()
{
    timer_restart(m_lastEditTimer);
}

float ZepEditor::GetLastEditElapsedTime() const
{
    return (float)timer_get_elapsed_seconds(m_lastEditTimer);
}

void ZepEditor::NextTabWindow()
{
    auto itr = std::find(m_tabWindows.begin(), m_tabWindows.end(), m_pActiveTabWindow);
    if (itr != m_tabWindows.end())
        itr++;

    if (itr == m_tabWindows.end())
    {
        itr = m_tabWindows.end() - 1;
    }
    m_pActiveTabWindow = *itr;
}

void ZepEditor::PreviousTabWindow()
{
    auto itr = std::find(m_tabWindows.begin(), m_tabWindows.end(), m_pActiveTabWindow);
    if (itr == m_tabWindows.end())
    {
        return;
    }

    if (itr != m_tabWindows.begin())
    {
        itr--;
    }

    m_pActiveTabWindow = *itr;
}

void ZepEditor::SetCurrentTabWindow(ZepTabWindow* pTabWindow)
{
    // Sanity
    auto itr = std::find(m_tabWindows.begin(), m_tabWindows.end(), pTabWindow);
    if (itr != m_tabWindows.end())
    {
        m_pActiveTabWindow = pTabWindow;
    }
}

ZepTabWindow* ZepEditor::GetActiveTabWindow() const
{
    return m_pActiveTabWindow;
}

ZepTabWindow* ZepEditor::AddTabWindow()
{
    auto pTabWindow = new ZepTabWindow(*this);
    m_tabWindows.push_back(pTabWindow);
    m_pActiveTabWindow = pTabWindow;

    auto pEmpty = GetEmptyBuffer("[No Name]", FileFlags::DefaultBuffer);
    pTabWindow->AddWindow(pEmpty, nullptr, false);

    return pTabWindow;
}

void ZepEditor::RequestQuit()
{
    Broadcast(std::make_shared<ZepMessage>(Msg::RequestQuit, "RequestQuit"));
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
    m_tabRects.clear();

    if (m_tabWindows.empty())
    {
        m_pActiveTabWindow = nullptr;

        // Reset the window state, but request a quit
        Reset();
        RequestQuit();
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

void ZepEditor::RegisterGlobalMode(std::shared_ptr<ZepMode> spMode)
{
    m_mapModes[spMode->Name()] = spMode;
}

void ZepEditor::SetGlobalMode(const std::string& mode)
{
    auto itrMode = m_mapModes.find(mode);
    if (itrMode != m_mapModes.end())
    {
        m_pCurrentMode = itrMode->second.get();
        m_pCurrentMode->Begin();
    }
}

ZepMode* ZepEditor::GetGlobalMode()
{
    // The 'Mode' is typically vim or normal and determines how editing is done in a panel
    if (!m_pCurrentMode && !m_mapModes.empty())
    {
        m_pCurrentMode = m_mapModes.begin()->second.get();
    }

    return m_pCurrentMode;
}

void ZepEditor::SetBufferSyntax(ZepBuffer& buffer) const
{
    std::string ext;
    std::string fileName;
    if (buffer.GetFilePath().has_filename() && buffer.GetFilePath().filename().has_extension())
    {
        ext = string_tolower(buffer.GetFilePath().filename().extension().string());
        fileName = string_tolower(buffer.GetFilePath().filename().string());
    }
    else
    {
        auto str = buffer.GetName();
        size_t dot_pos = str.find_last_of(".");
        if (dot_pos != std::string::npos)
        {
            ext = string_tolower(str.substr(dot_pos, str.length() - dot_pos));
        }
    }

    // first check file name
    if (!fileName.empty())
    {
        auto itr = m_mapSyntax.find(fileName);
        if (itr != m_mapSyntax.end())
        {
            buffer.SetSyntaxProvider(itr->second);
            return;
        }
    }

    auto itr = m_mapSyntax.find(ext);
    if (itr != m_mapSyntax.end())
    {
        buffer.SetSyntaxProvider(itr->second);
    }
    else
    {
        itr = m_mapSyntax.find(string_tolower(buffer.GetName()));
        if (itr != m_mapSyntax.end())
        {
            buffer.SetSyntaxProvider(itr->second);
        }
        else
        {
            buffer.SetSyntaxProvider(SyntaxProvider{});
        }
    }
}

void ZepEditor::RegisterSyntaxFactory(const std::vector<std::string>& mappings, SyntaxProvider provider)
{
    for (auto& m : mappings)
    {
        m_mapSyntax[string_tolower(m)] = provider;
    }
}

// Inform clients of an event in the buffer
bool ZepEditor::Broadcast(std::shared_ptr<ZepMessage> message)
{
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

void ZepEditor::InitDataGrid(ZepBuffer& buffer, const NVec2i& dimensions)
{
    std::ostringstream str;
    for (int column = 0; column < dimensions.y; column++)
    {
        for (int row = 0; row < dimensions.x; row++)
        {
            str << ".";
        }
        str << "\n";
    }
    buffer.SetText(str.str());
}

// Do any special buffer processing
void ZepEditor::InitBuffer(ZepBuffer& buffer)
{
    auto ext = buffer.GetFileExtension();
    if (ext == ".orca")
    {
        buffer.SetBufferType(BufferType::DataGrid);

        if (buffer.GetFilePath().empty())
        {
            const NVec2i DefaultDataGridSize = NVec2i(20, 15);
            InitDataGrid(buffer, DefaultDataGridSize);
        }

        // Use vim mode for editing orca buffers
        auto pMode = std::make_shared<ZepMode_Vim>(*this);
        buffer.SetMode(pMode);
    }
}

ZepBuffer* ZepEditor::CreateNewBuffer(const std::string& str)
{
    auto pBuffer = std::make_shared<ZepBuffer>(*this, str);

    // For a new buffer, set the syntax based on the string name
    SetBufferSyntax(*pBuffer);

    m_buffers.push_front(pBuffer);

    InitBuffer(*pBuffer);

    return pBuffer.get();
}

ZepBuffer* ZepEditor::CreateNewBuffer(const ZepPath& path)
{
    auto pBuffer = std::make_shared<ZepBuffer>(*this, path);
    m_buffers.push_front(pBuffer);

    InitBuffer(*pBuffer);

    return pBuffer.get();
}

ZepBuffer* ZepEditor::GetMRUBuffer() const
{
    return m_buffers.front().get();
}

void ZepEditor::ReadClipboard()
{
    auto pMsg = std::make_shared<ZepMessage>(Msg::GetClipBoard);
    Broadcast(pMsg);
    if (pMsg->handled)
    {
        m_registers["+"] = pMsg->str;
        m_registers["*"] = pMsg->str;
    }
}

void ZepEditor::WriteClipboard()
{
    auto pMsg = std::make_shared<ZepMessage>(Msg::SetClipBoard);
    pMsg->str = m_registers["+"].text;
    Broadcast(pMsg);
}

void ZepEditor::SetRegister(const std::string& reg, const Register& val)
{
    m_registers[reg] = val;
    if (reg == "+" || reg == "*")
    {
        WriteClipboard();
    }
}

void ZepEditor::SetRegister(const char reg, const Register& val)
{
    std::string str({ reg });
    m_registers[str] = val;
    if (reg == '+' || reg == '*')
    {
        WriteClipboard();
    }
}

void ZepEditor::SetRegister(const std::string& reg, const char* pszText)
{
    m_registers[reg] = Register(pszText);
    if (reg == "+" || reg == "*")
    {
        WriteClipboard();
    }
}

void ZepEditor::SetRegister(const char reg, const char* pszText)
{
    std::string str({ reg });
    m_registers[str] = Register(pszText);
    if (reg == '+' || reg == '*')
    {
        WriteClipboard();
    }
}

Register& ZepEditor::GetRegister(const std::string& reg)
{
    if (reg == "+" || reg == "*")
    {
        ReadClipboard();
    }
    return m_registers[reg];
}

Register& ZepEditor::GetRegister(const char reg)
{
    if (reg == '+' || reg == '*')
    {
        ReadClipboard();
    }
    std::string str({ reg });
    return m_registers[str];
}
const tRegisters& ZepEditor::GetRegisters()
{
    ReadClipboard();
    return m_registers;
}

void ZepEditor::Notify(std::shared_ptr<ZepMessage> pMsg)
{
    if (pMsg->messageId == Msg::MouseDown)
    {
        for (auto& windowRect : m_tabRects)
        {
            if (windowRect.second.Contains(pMsg->pos))
            {
                SetCurrentTabWindow(windowRect.first);
            }
        }
    }
}

std::string ZepEditor::GetCommandText() const
{
    std::ostringstream str;
    bool start = true;
    for (auto& line : m_commandLines)
    {
        if (!start)
            str << "\n";
        start = false;
        str << line;
    }
    return str.str();
}

void ZepEditor::SetCommandText(const std::string& strCommand)
{
    m_commandLines = string_split(strCommand, "\n\r");
    if (m_commandLines.empty())
    {
        m_commandLines.push_back("");
    }
    m_bRegionsChanged = true;
}

void ZepEditor::RequestRefresh()
{
    m_bPendingRefresh = true;
}

bool ZepEditor::RefreshRequired()
{
    // Allow any components to update themselves
    Broadcast(std::make_shared<ZepMessage>(Msg::Tick));

    if (m_bPendingRefresh || m_lastCursorBlink != GetCursorBlinkState())
    {
        m_bPendingRefresh = false;
        return true;
    }

    return false;
}

bool ZepEditor::GetCursorBlinkState() const
{
    m_lastCursorBlink = (int(timer_get_elapsed_seconds(m_cursorTimer) * 1.75f) & 1) ? true : false;
    return m_lastCursorBlink;
}

void ZepEditor::SetDisplayRegion(const NVec2f& topLeft, const NVec2f& bottomRight)
{
    m_editorRegion->rect.topLeftPx = topLeft;
    m_editorRegion->rect.bottomRightPx = bottomRight;
    UpdateSize();
}

void ZepEditor::UpdateSize()
{
    auto commandCount = GetCommandLines().size();
    const float commandSize = m_pDisplay->GetFontHeightPixels() * commandCount + textBorder * 2.0f;
    auto displaySize = m_editorRegion->rect.Size();

    // Regions
    m_commandRegion->fixed_size = NVec2f(0.0f, commandSize);
    m_commandRegion->flags = RegionFlags::Fixed;

    // Add tabs for extra windows
    if (GetTabWindows().size() > 1)
    {
        m_tabRegion->fixed_size = NVec2f(0.0f, m_pDisplay->GetFontHeightPixels() + textBorder * 2);
        m_tabRegion->flags = RegionFlags::Fixed;
    }
    else
    {
        m_tabRegion->fixed_size = NVec2f(0.0f, 0.0f);
        m_tabRegion->flags = RegionFlags::Fixed;
    }

    m_tabContentRegion->flags = RegionFlags::Expanding;

    LayoutRegion(*m_editorRegion);

    if (GetActiveTabWindow())
    {
        GetActiveTabWindow()->SetDisplayRegion(m_tabContentRegion->rect);
    }
}

void ZepEditor::Display()
{
    UpdateWindowState();

    if (m_bRegionsChanged)
    {
        m_bRegionsChanged = false;
        UpdateSize();
    }

    // Command plus output
    auto& commandLines = GetCommandLines();

    long commandCount = long(commandLines.size());
    const float commandSize = m_pDisplay->GetFontHeightPixels() * commandCount + textBorder * 2.0f;

    auto displaySize = m_editorRegion->rect.Size();

    auto commandSpace = commandCount;
    commandSpace = std::max(commandCount, 0l);

    // This fill will effectively fill the region around the tabs in Normal mode
    if (GetConfig().style == EditorStyle::Normal)
    {
        GetDisplay().DrawRectFilled(m_editorRegion->rect, GetTheme().GetColor(ThemeColor::Background));
    }

    // Background rect for CommandLine
    if (!GetCommandText().empty() || (GetConfig().autoHideCommandRegion == false))
    {
        m_pDisplay->DrawRectFilled(m_commandRegion->rect, GetTheme().GetColor(ThemeColor::Background));
    }

    // Draw command text
    auto screenPosYPx = m_commandRegion->rect.topLeftPx + NVec2f(0.0f, textBorder);
    for (int i = 0; i < commandSpace; i++)
    {
        if (!commandLines[i].empty())
        {
            auto textSize = m_pDisplay->GetTextSize((const utf8*)commandLines[i].c_str(), (const utf8*)commandLines[i].c_str() + commandLines[i].size());
            m_pDisplay->DrawChars(screenPosYPx, GetTheme().GetColor(ThemeColor::Text), (const utf8*)commandLines[i].c_str());
        }

        screenPosYPx.y += m_pDisplay->GetFontHeightPixels();
        screenPosYPx.x = m_commandRegion->rect.topLeftPx.x;
    }

    if (GetConfig().style == EditorStyle::Normal)
    {
        // A line along the bottom of the tab region
        m_pDisplay->DrawRectFilled(
            NRectf(NVec2f(m_tabRegion->rect.Left(), m_tabRegion->rect.Bottom() - 1), NVec2f(m_tabRegion->rect.Right(), m_tabRegion->rect.Bottom())), GetTheme().GetColor(ThemeColor::TabInactive));
    }

    m_tabRects.clear();
    if (GetTabWindows().size() > 1)
    {
        // Tab region
        // TODO Handle it when tabs are bigger than the available width!
        NVec2f currentTab = m_tabRegion->rect.topLeftPx;
        for (auto& window : GetTabWindows())
        {
            // Show active buffer in tab as tab name
            auto& buffer = window->GetActiveWindow()->GetBuffer();
            std::string name = buffer.GetName();
            if (m_config.shortTabNames)
            {
                auto pos = name.find_last_of('.');
                if (pos != std::string::npos)
                {
                    name = name.substr(0, pos);
                }
            }

            auto tabColor = GetTheme().GetColor(ThemeColor::TabActive);
            if (buffer.TestFlags(FileFlags::HasWarnings))
            {
                tabColor = GetTheme().GetColor(ThemeColor::Warning);
            }
            // Errors win for coloring
            if (buffer.TestFlags(FileFlags::HasErrors))
            {
                tabColor = GetTheme().GetColor(ThemeColor::Error);
            }
            if (window != GetActiveTabWindow())
            {
                // Desaturate unselected ones
                tabColor = tabColor * .55f;
                tabColor.w = 1.0f;
            }
            auto tabLength = m_pDisplay->GetTextSize((const utf8*)name.c_str()).x + textBorder * 2;

            // Tab background rect
            NRectf tabRect(currentTab, currentTab + NVec2f(tabLength, m_tabRegion->rect.Height()));
            m_pDisplay->DrawRectFilled(tabRect, tabColor);

            // Tab text
            m_pDisplay->DrawChars(currentTab + NVec2f(textBorder, textBorder), NVec4f(1.0f), (const utf8*)name.c_str());

            currentTab.x += tabLength + textBorder;
            m_tabRects[window] = tabRect;
        }
    }

    // Display the tab
    if (GetActiveTabWindow())
    {
        GetActiveTabWindow()->Display();
    }
}

ZepTheme& ZepEditor::GetTheme() const
{
    return *m_spTheme;
}

bool ZepEditor::OnMouseMove(const NVec2f& mousePos)
{
    m_mousePos = mousePos;
    bool handled = Broadcast(std::make_shared<ZepMessage>(Msg::MouseMove, mousePos));
    m_bPendingRefresh = true;
    return handled;
}

bool ZepEditor::OnMouseDown(const NVec2f& mousePos, ZepMouseButton button)
{
    m_mousePos = mousePos;
    bool handled = Broadcast(std::make_shared<ZepMessage>(Msg::MouseDown, mousePos, button));
    m_bPendingRefresh = true;
    return handled;
}

bool ZepEditor::OnMouseUp(const NVec2f& mousePos, ZepMouseButton button)
{
    m_mousePos = mousePos;
    bool handled = Broadcast(std::make_shared<ZepMessage>(Msg::MouseUp, mousePos, button));
    m_bPendingRefresh = true;
    return handled;
}

const NVec2f ZepEditor::GetMousePos() const
{
    return m_mousePos;
}

void ZepEditor::SetPixelScale(float scale)
{
    m_pixelScale = scale;
}

float ZepEditor::GetPixelScale() const
{
    return m_pixelScale;
}

} // namespace Zep
