#include "utils.h"
#include "utils/logger.h"

#include <sstream>

#include "ui_manager.h"

#include "sdl.h"

using namespace Mgfx;

// Statics
uint64_t UIMessage::CurrentID = 1;

namespace
{
uint64_t InvalidMessageID = 0;
}

void UIMessage::Log()
{
    /*    uint32_t level = easyloggingERROR;
        if (m_type & MessageType::Warning)
        {
            level = WARNING;
        }
        */

    std::ostringstream str;
    try
    {
        if (!m_file.empty())
        {
            str << m_file.string();
        }
    }
    catch (fs::filesystem_error&)
    {
        // Ignore file errors
    }

    if (m_line != -1)
    {
        str << "(" << m_line;
        if (m_columnRange.start != -1)
        {
            str << "," << m_columnRange.start;
            if (m_columnRange.end != -1)
            {
                str << "-" << m_columnRange.end;
            }
        }
        str << "): ";
    }
    else
    {
        // Just a file, no line
        if (!m_file.empty())
        {
            str << ": ";
        }
    }
    str << m_message;
    LOG(DEBUG) << str.str();
}

UIManager::UIManager()
{

}

UIManager& UIManager::Instance()
{
    static UIManager manager;
    return manager;
}

uint64_t UIManager::AddMessage(uint32_t type, const std::string& message, const fs::path& file, int32_t line, const ColumnRange& column)
{
    auto spMessage = std::make_shared<UIMessage>(type, message, file, line, column);
    spMessage->Log();
    if (type & MessageType::Task ||
        !file.empty())
    {
        m_taskMessages[spMessage->m_id] = spMessage;
    }

    if (!file.empty())
    {
        m_fileMessages[spMessage->m_file].push_back(spMessage->m_id);
    }

    if (type & MessageType::System)
    {
        SDL_MessageBoxButtonData button;
        button.flags = SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT;
        button.buttonid = 0;
        button.text = "OK";

        SDL_MessageBoxData mbData;
        mbData.buttons = &button;
        mbData.colorScheme = nullptr;
        if (type & MessageType::Error)
        {
            mbData.flags = SDL_MessageBoxFlags::SDL_MESSAGEBOX_ERROR;
            mbData.title = "Error";
        }
        else if (type & MessageType::Warning)
        {
            mbData.flags = SDL_MessageBoxFlags::SDL_MESSAGEBOX_WARNING;
            mbData.title = "Warning";
        }
        else if (type & MessageType::Info)
        {
            mbData.flags = SDL_MessageBoxFlags::SDL_MESSAGEBOX_INFORMATION;
            mbData.title = "Information";
        }
        mbData.message = spMessage->m_message.c_str();
        mbData.numbuttons = 1;
        mbData.window = nullptr;

        int buttonID = 0;
        SDL_ShowMessageBox(&mbData, &buttonID);
    }
    return spMessage->m_id;
}

// Remove a message for a given ID
void UIManager::RemoveMessage(uint64_t id)
{
    auto itrFound = m_taskMessages.find(id);
    if (itrFound != m_taskMessages.end())
    {
        if (!itrFound->second->m_file.empty())
        {
            m_fileMessages.erase(itrFound->second->m_file);
        }
    }
}

// Remove all messages associated with a file
void UIManager::ClearFileMessages(fs::path path)
{
    while(!m_fileMessages[path].empty())
    {
        RemoveMessage(m_fileMessages[path][0]);
    }
}
