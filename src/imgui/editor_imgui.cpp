#include <string>
#include <imgui.h>
#include "editor.h"
#include "display_imgui.h"
#include "syntax_glsl.h"
#include "mode_vim.h"
#include "mode_standard.h"
#include "editor_imgui.h"
#include "usb_hid_keys.h"
#include "SDL.h"
#include "tab_window.h"

namespace Zep
{

ZepEditor_ImGui::ZepEditor_ImGui()
{
    m_spDisplay = std::make_unique<ZepDisplay_ImGui>(*this);
}

/*
void ZepEditor_ImGui::UpdateWindows()
{

}
*/

void ZepEditor_ImGui::Display(const NVec2f& pos, const NVec2f& size)
{
    m_spDisplay->SetDisplaySize(pos, pos + size);
    m_spDisplay->Display();

    HandleInput();
}

void ZepEditor_ImGui::HandleInput()
{
    auto& io = ImGui::GetIO();

    bool inputChanged = false;
    bool handled = false;

    uint32_t mod = 0;
    
    if (io.KeyCtrl)
    {
        mod |= ModifierKey::Ctrl;
    }
    if (io.KeyShift)
    {
        mod |= ModifierKey::Shift;
    }

    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::TAB, mod);
    }
    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::ESCAPE, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::RETURN, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::DEL, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::HOME, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::END, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::BACKSPACE, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::RIGHT, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::LEFT, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::UP, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::DOWN, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::PAGEDOWN, mod);
    }
    else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
    {
        GetCurrentMode()->AddKeyPress(ExtKeys::PAGEUP, mod);
    }
    else if (io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(KEY_1))
        {
            SetMode(StandardMode);
            handled = true;
        }
        else if (ImGui::IsKeyPressed(KEY_2))
        {
            SetMode(VimMode);
            handled = true;
        }
        else
        {
            for (int ch = SDL_SCANCODE_A; ch <= SDL_SCANCODE_Z; ch++)
            {
                if (ImGui::IsKeyPressed(ch))
                {
                    GetCurrentMode()->AddKeyPress((ch - SDL_SCANCODE_A) + 'a', mod);
                    handled = true;
                }
            }

            if (ImGui::IsKeyPressed(KEY_SPACE))
            {
                GetCurrentMode()->AddKeyPress(' ', mod);
                handled = true;
            }
        }
    }

    if (!handled)
    {
        for (int n = 0; n < (sizeof(io.InputCharacters) / sizeof(*io.InputCharacters)) && io.InputCharacters[n]; n++)
        {
            GetCurrentMode()->AddKeyPress(io.InputCharacters[n], mod);
        }
    }

}

} // Zep
