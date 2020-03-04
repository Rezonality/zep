#include <cctype>
#include <sstream>

#include "zep/mcommon/animation/timer.h"
#include "zep/mcommon/logger.h"
#include "zep/mcommon/string/stringutils.h"

#include "zep/keymap.h"
#include "zep/tab_window.h"
#include "zep/theme.h"

#include "mode_orca.h"
#include "syntax_orca.h"

namespace Zep
{

ZepMode_Orca::ZepMode_Orca(ZepEditor& editor)
    : ZepMode_Vim(editor)
{
}

ZepMode_Orca::~ZepMode_Orca()
{
}

void ZepMode_Orca::SetupKeyMaps()
{
    // Standard choices
    AddGlobalKeyMaps();
    AddNavigationKeyMaps(true);
    AddSearchKeyMaps();
    AddOverStrikeMaps();
    AddCopyMaps();

    // Mode switching
    AddKeyMapWithCountRegisters({ &m_normalMap, &m_visualMap }, { "<Escape>" }, id_NormalMode);
    keymap_add({ &m_insertMap }, { "jk" }, id_NormalMode);
    keymap_add({ &m_insertMap }, { "<Escape>" }, id_NormalMode);
    AddKeyMapWithCountRegisters({ &m_visualMap }, { ":", "/", "?" }, id_ExMode);

    AddKeyMapWithCountRegisters({ &m_normalMap }, { "<Return>" }, id_MotionNextFirstChar);

    // Undo redo
    AddKeyMapWithCountRegisters({ &m_normalMap }, { "<C-r>" }, id_Redo);
    AddKeyMapWithCountRegisters({ &m_normalMap }, { "<C-z>", "u" }, id_Undo);
}

void ZepMode_Orca::Begin(ZepWindow* pWindow)
{
    ZepMode_Vim::Begin(pWindow);
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
    ZepMode_Vim::PreDisplay(window);

}

static std::set<std::string> orca_keywords = {};
static std::set<std::string> orca_identifiers = {};

void ZepMode_Orca::Register(ZepEditor& editor)
{
    editor.RegisterSyntaxFactory(
        { ".orca" },
        SyntaxProvider{ "orca", tSyntaxFactory([](ZepBuffer* pBuffer) {
                           return std::make_shared<ZepSyntax_Orca>(*pBuffer, orca_keywords, orca_identifiers, ZepSyntaxFlags::CaseInsensitive);
                       }) });

    editor.RegisterBufferMode(".orca", std::make_shared<ZepMode_Orca>(editor));
}

} // namespace Zep
