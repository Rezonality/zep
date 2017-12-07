#include "editor.h"
#include "buffer.h"
#include "display.h"
#include "mode_vim.h"
#include "mode_standard.h"
#include "syntax_glsl.h"
#include "syntax.h"

namespace Zep
{

const char* Msg_GetClipBoard = "GetClipboard";
const char* Msg_SetClipBoard = "SetClipboard";
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


ZepEditor::ZepEditor(uint32_t flags)
    : m_flags(flags)
{
    RegisterMode(VimMode, std::make_shared<ZepMode_Vim>(*this));
    RegisterMode(StandardMode, std::make_shared<ZepMode_Standard>(*this));
    SetMode(VimMode);

    AddBuffer("Scratch");
    
    RegisterSyntaxFactory("vert", tSyntaxFactory([](ZepBuffer* pBuffer) 
    {
        return std::static_pointer_cast<ZepSyntax>(std::make_shared<ZepSyntaxGlsl>(*pBuffer)); 
    }));
}

ZepEditor::~ZepEditor()
{

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
        m_pCurrentMode->EnterMode();
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
    auto spBuffer = std::make_shared<ZepBuffer>(*this, str);
    m_buffers.push_front(spBuffer);

    auto extOffset = str.find_last_of('.');
    if (extOffset != std::string::npos)
    {
        auto itrFactory = m_mapSyntax.find(str.substr(extOffset + 1, str.size() - extOffset));
        if (itrFactory != m_mapSyntax.end())
        {
            spBuffer->SetSyntax(itrFactory->second(spBuffer.get()));
        }
    }
    return spBuffer.get();
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

} // namespace Zep
