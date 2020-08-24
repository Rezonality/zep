#include <cctype>
#include <sstream>
#include <thread>

#include "zep/mcommon/animation/timer.h"
#include "zep/mcommon/logger.h"
#include "zep/mcommon/string/stringutils.h"

#include "zep/buffer.h"
#include "zep/keymap.h"
#include "zep/tab_window.h"
#include "zep/theme.h"
#include "zep/window.h"

#include "mode_orca.h"
#include "syntax_orca.h"

#include "orca.h"

#include <fmt/format.h>

using namespace MUtils;
namespace Zep
{

DECLARE_COMMANDID(Orca_FrameStep)
DECLARE_COMMANDID(Orca_ResetFrameCount)
DECLARE_COMMANDID(Orca_TogglePause)
DECLARE_COMMANDID(Orca_Delete)

ZepMode_Orca::ZepMode_Orca(ZepEditor& editor)
    : ZepMode_Vim(editor)
{
}

ZepMode_Orca::~ZepMode_Orca()
{
    GetEditor().UnRegisterCallback(this);

    // Shut down any dangling orca threads
    for(auto itrPair : m_mapOrca) 
    {
        itrPair.second->Quit();
    }
    m_mapOrca.clear();
}

void ZepMode_Orca::Init()
{
    ZepMode_Vim::Init();

    GetEditor().RegisterCallback(this);
}

void ZepMode_Orca::SetupKeyMaps()
{
    std::vector<KeyMap*> navigationMaps = { &m_normalMap, &m_visualMap };

    // Up/Down/Left/Right
    AddKeyMapWithCountRegisters(navigationMaps, { "<Down>" }, id_MotionDown);
    AddKeyMapWithCountRegisters(navigationMaps, { "<Up>" }, id_MotionUp);
    AddKeyMapWithCountRegisters(navigationMaps, { "<Right>" }, id_MotionRight);
    AddKeyMapWithCountRegisters(navigationMaps, { "<Left>" }, id_MotionLeft);

    keymap_add({ &m_insertMap }, { "jk" }, id_NormalMode);
    keymap_add({ &m_insertMap }, { "<Escape>" }, id_NormalMode);

    // Page Motions
    AddKeyMapWithCountRegisters(navigationMaps, { "<C-f>", "<PageDown>" }, id_MotionPageForward);
    AddKeyMapWithCountRegisters(navigationMaps, { "<C-b>", "<PageUp>" }, id_MotionPageBackward);
    AddKeyMapWithCountRegisters(navigationMaps, { "<C-d>" }, id_MotionHalfPageForward);
    AddKeyMapWithCountRegisters(navigationMaps, { "<C-u>" }, id_MotionHalfPageBackward);

    // Line Motions
    //AddKeyMapWithCountRegisters(navigationMaps, { "$" }, id_MotionLineEnd);
    //AddKeyMapWithCountRegisters(navigationMaps, { "^" }, id_MotionLineFirstChar);

    // Navigate between splits
    keymap_add(navigationMaps, { "<C-j>" }, id_MotionDownSplit);
    keymap_add(navigationMaps, { "<C-l>" }, id_MotionRightSplit);
    keymap_add(navigationMaps, { "<C-k>" }, id_MotionUpSplit);
    keymap_add(navigationMaps, { "<C-h>" }, id_MotionLeftSplit);

    keymap_add({ &m_normalMap }, { "<F8>" }, id_MotionNextMarker);
    keymap_add({ &m_normalMap }, { "<S-F8>" }, id_MotionPreviousMarker);

    keymap_add({ &m_normalMap }, { "<C-p>", "<C-,>" }, id_QuickSearch);
    keymap_add({ &m_normalMap }, { "~", "/", "?" }, id_ExMode);
    
    //keymap_add({ &m_normalMap }, { "H" }, id_PreviousTabWindow);
    //keymap_add({ &m_normalMap }, { "L" }, id_NextTabWindow);

    keymap_add({ &m_normalMap }, { "<C-i><C-o>" }, id_SwitchToAlternateFile);
    keymap_add({ &m_normalMap }, { "+" }, id_FontBigger);
    keymap_add({ &m_normalMap }, { "-" }, id_FontSmaller);

    // Mode switching
    AddKeyMapWithCountRegisters({ &m_normalMap, &m_visualMap }, { "<Escape>" }, id_NormalMode);
    AddKeyMapWithCountRegisters({ &m_visualMap }, { "~", "/", "?" }, id_ExMode);

    AddKeyMapWithCountRegisters({ &m_normalMap }, { "<Return>" }, id_MotionNextFirstChar);

    // Undo redo
    AddKeyMapWithCountRegisters({ &m_normalMap }, { "<C-r>" }, id_Redo);
    AddKeyMapWithCountRegisters({ &m_normalMap }, { "<C-z>" }, id_Undo);

    keymap_add({ &m_normalMap }, { "<C-f>" }, id_Orca_FrameStep);
    //keymap_add({ &m_normalMap }, { "<S-C-R>" }, id_Orca_ResetFrameCount); (broken)
    keymap_add({ &m_normalMap }, { " " }, id_Orca_TogglePause);
    
    keymap_add({ &m_normalMap }, { "<Backspace>" }, id_Orca_Delete);
}


Orca* ZepMode_Orca::GetCurrentOrca() const
{
    if (!m_pCurrentWindow)
        return nullptr;

    auto pBuffer = &m_pCurrentWindow->GetBuffer();
    auto itr = m_mapOrca.find(pBuffer);
    if (itr != m_mapOrca.end())
    {
        return itr->second.get();
    }
    return nullptr;
}

bool ZepMode_Orca::HandleSpecialCommand(CommandContext& context)
{
    auto id = context.keymap.foundMapping;

    auto pOrca = GetCurrentOrca();
    if (!pOrca)
    {
        return false;
    }

    if (id == id_Orca_TogglePause)
    {
        pOrca->Enable(!pOrca->IsEnabled());
        return true;
    }
    else if (id == id_Orca_FrameStep)
    {
        // Step at current rate; this just queues a step to occur
        pOrca->Step();

        /// We don't want to interfere with the regular tick of the time provider, so manually tick orca
        // TODO: How to frame step?
        //TimeProvider::Instance().Beat();

        return true;
    }
    else if (id == id_Orca_ResetFrameCount)
    {
        pOrca->SetFrame(0);
        return true;
    }
    else if (id == id_Orca_Delete)
    {
        auto cmd = std::make_shared<ZepCommand_ReplaceRange>(
            context.buffer,
            ReplaceRangeMode::Replace,
            context.bufferCursor,
            context.bufferCursor + 1,
            ".",
            context.bufferCursor,
            context.bufferCursor);
        AddCommand(cmd);
        return true;
    }
    return false;
}

void ZepMode_Orca::Begin(ZepWindow* pWindow)
{
    ZepMode_Vim::Begin(pWindow);

    if (pWindow == nullptr)
    {
        return;
    }

    m_pCurrentWindow = pWindow;
    auto pBuffer = &pWindow->GetBuffer();
    auto itr = m_mapOrca.find(pBuffer);
    if (itr != m_mapOrca.end())
    {
        // Already setup
        return;
    }

    // Make a new thread for this buffer
    auto pOrca = std::make_shared<Orca>();
    m_mapOrca[pBuffer] = pOrca;
    pOrca->Init(GetEditor());
    pOrca->ReadFromBuffer(pBuffer);
    pOrca->Enable(true);
}

std::vector<Airline> ZepMode_Orca::GetAirlines(ZepWindow& win) const
{
    std::vector<Airline> airlines;

    auto pBuffer = &win.GetBuffer();
    auto itr = m_mapOrca.find(pBuffer);
    if (itr == m_mapOrca.end())
    {
        return airlines;
    }

    auto& orca = itr->second;

    Airline line;


    auto& tp = TimeProvider::Instance();


    line.leftBoxes.push_back(AirBox{ fmt::format("Grid {}x{}", orca->GetSize().x, orca->GetSize().y), win.FilterActiveColor(win.GetBuffer().GetTheme().GetColor(ThemeColor::UniqueColor0)) });
    line.leftBoxes.push_back(AirBox{ fmt::format("{}f {:0.2f}{}", orca->GetFrame(), tp.GetTempo(), orca->IsZeroQuantum() ? "*" : " "), win.FilterActiveColor(win.GetBuffer().GetTheme().GetColor(ThemeColor::UniqueColor2)) });
    airlines.push_back(line);

    return airlines;
}

// Modify flags for the window in this mode
uint32_t ZepMode_Orca::ModifyWindowFlags(uint32_t flags)
{
    flags = ZClearFlags(flags, WindowFlags::WrapText);
    flags = ZClearFlags(flags, WindowFlags::ShowLineNumbers);
    flags = ZClearFlags(flags, WindowFlags::ShowIndicators);
    flags = ZSetFlags(flags, WindowFlags::GridStyle);
    flags = ZSetFlags(flags, WindowFlags::HideScrollBar);
    //flags = ZSetFlags(flags, WindowFlags::HideSplitMark);
    return flags;
}

void ZepMode_Orca::PreDisplay(ZepWindow& window)
{
    auto pBuffer = &window.GetBuffer();
    auto itrFound = m_mapOrca.find(pBuffer);
    if (itrFound != m_mapOrca.end())
    {
        // We are on the UI thread here
        itrFound->second->WriteToBuffer(pBuffer, window);
    }

    if (pBuffer->GetLineCount() > 0)
    {
        m_bufferHeight = pBuffer->GetLineCount();

        ByteRange range;
        pBuffer->GetLineOffsets(0, range);
        m_bufferWidth = range.second - range.first;
    }
    else
    {
        m_bufferWidth = m_bufferHeight = 0;
    }

    ZepMode_Vim::PreDisplay(window);
}

static std::unordered_set<std::string> orca_keywords = {};
static std::unordered_set<std::string> orca_identifiers = {};

std::shared_ptr<ZepMode_Orca> ZepMode_Orca::Register(ZepEditor& editor)
{
    editor.RegisterSyntaxFactory(
        { ".orca" },
        SyntaxProvider{ "orca", tSyntaxFactory([](ZepBuffer* pBuffer) {
                           return std::make_shared<ZepSyntax_Orca>(*pBuffer, orca_keywords, orca_identifiers, ZepSyntaxFlags::CaseInsensitive);
                       }) });

    auto spOrca = std::make_shared<ZepMode_Orca>(editor);
    editor.RegisterBufferMode(".orca", spOrca);
    return spOrca;
}

void ZepMode_Orca::Notify(std::shared_ptr<ZepMessage> spMsg)
{
    // Handle any interesting buffer messages
    if (spMsg->messageId == Msg::Buffer)
    {
        auto spBufferMsg = std::static_pointer_cast<BufferMessage>(spMsg);
        if (spBufferMsg->type == BufferMessageType::PreBufferChange)
        {
            return;
        }

        auto itrFound = m_mapOrca.find(spBufferMsg->pBuffer);
        if (itrFound == m_mapOrca.end())
        {
            return;
        }

        itrFound->second->ReadFromBuffer(spBufferMsg->pBuffer);
    }
}

bool ZepMode_Orca::HandleIgnoredInput(CommandContext& context)
{
    if (context.fullCommand.size() == 1)
    {
        const std::string& ch = context.fullCommand;
        if (std::isgraph(ch[0]))
        {
            auto cmd = std::make_shared<ZepCommand_ReplaceRange>(
                context.buffer,
                ReplaceRangeMode::Replace,
                context.bufferCursor,
                context.bufferCursor + 1,
                ch,
                context.bufferCursor,
                context.bufferCursor);
            AddCommand(cmd);
            return true;
        }
    }
    return false;
}

} // namespace Zep
