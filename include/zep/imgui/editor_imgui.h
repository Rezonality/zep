#pragma once
#include <string>

#include "zep/imgui/display_imgui.h"
#include "zep/imgui/usb_hid_keys.h"

#include "zep/editor.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/syntax.h"
#include "zep/tab_window.h"
#include "zep/window.h"

namespace Zep
{

class ZepDisplay_ImGui;
class ZepTabWindow;
class ZepEditor_ImGui : public ZepEditor
{
public:
    ZepEditor_ImGui(const ZepPath& root, const NVec2f& pixelScale, uint32_t flags = 0, IZepFileSystem* pFileSystem = nullptr)
        : ZepEditor(new ZepDisplay_ImGui(), root, flags, pFileSystem)
    {
    }

    void HandleInput()
    {
        auto& io = ImGui::GetIO();

        bool handled = false;

        uint32_t mod = 0;

        static std::map<int, int> MapUSBKeys =
        {
            { KEY_F1, ExtKeys::F1},
            { KEY_F2, ExtKeys::F2},
            { KEY_F3, ExtKeys::F3},
            { KEY_F4, ExtKeys::F4},
            { KEY_F5, ExtKeys::F5},
            { KEY_F6, ExtKeys::F6},
            { KEY_F7, ExtKeys::F7},
            { KEY_F8, ExtKeys::F8},
            { KEY_F9, ExtKeys::F9},
            { KEY_F10, ExtKeys::F10},
            { KEY_F11, ExtKeys::F11},
            { KEY_F12, ExtKeys::F12}
        };
        if (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)
        {
            OnMouseMove(toNVec2f(io.MousePos));
        }

        if (io.MouseClicked[0])
        {
            if (OnMouseDown(toNVec2f(io.MousePos), ZepMouseButton::Left))
            {
                // Hide the mouse click from imgui if we handled it
                io.MouseClicked[0] = false;
            }
        }

        if (io.MouseClicked[1])
        {
            if (OnMouseDown(toNVec2f(io.MousePos), ZepMouseButton::Right))
            {
                // Hide the mouse click from imgui if we handled it
                io.MouseClicked[0] = false;
            }
        }

        if (io.MouseReleased[0])
        {
            if (OnMouseUp(toNVec2f(io.MousePos), ZepMouseButton::Left))
            {
                // Hide the mouse click from imgui if we handled it
                io.MouseClicked[0] = false;
            }
        }

        if (io.MouseReleased[1])
        {
            if (OnMouseUp(toNVec2f(io.MousePos), ZepMouseButton::Right))
            {
                // Hide the mouse click from imgui if we handled it
                io.MouseClicked[0] = false;
            }
        }
        if (io.MouseWheel)
        {
            if (OnMouseWheel(toNVec2f(io.MousePos), io.MouseWheel))
            {
                // Hide the mouse scroll from imgui if we handled it
                io.MouseWheel = 0;
            }
        }


        if (io.KeyCtrl)
        {
            mod |= ModifierKey::Ctrl;
        }
        if (io.KeyShift)
        {
            mod |= ModifierKey::Shift;
        }

        auto pBuffer = GetActiveBuffer();
        if (!pBuffer) return;

        // Check USB Keys
        for (auto& usbKey : MapUSBKeys)
        {
            if (ImGui::IsKeyPressed(ImGuiKey(usbKey.first)))
            {
                pBuffer->GetMode()->AddKeyPress(usbKey.second, mod);
                return;
            }
        }

        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::TAB, mod);
            return;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::ESCAPE, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::RETURN, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::DEL, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::HOME, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::END, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::BACKSPACE, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::RIGHT, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::LEFT, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::UP, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::DOWN, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::PAGEDOWN, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
        {
            pBuffer->GetMode()->AddKeyPress(ExtKeys::PAGEUP, mod);
            return;
        }
        else if (io.KeyCtrl)
        {
            // SDL Remaps to its own scancodes; and since we can't look them up in the standard IMGui list
            // without modifying the ImGui base code, we have special handling here for CTRL.
            // For the Win32 case, we use VK_A (ASCII) is handled below
#if defined(_SDL_H) || defined(ZEP_USE_SDL)
            if (ImGui::IsKeyPressed(ImGuiKey(KEY_1)))
            {
                SetGlobalMode(ZepMode_Standard::StaticName());
                handled = true;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey(KEY_2)))
            {
                SetGlobalMode(ZepMode_Vim::StaticName());
                handled = true;
            }
            else
            {
                for (int ch = KEY_1; ch <= KEY_0; ch++)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey(ch)))
                    {
                        pBuffer->GetMode()->AddKeyPress(ch == KEY_0 ? '0' : ch - KEY_1 + '1', mod);
                        handled = true;
                    }
                }
                for (int ch = KEY_A; ch <= KEY_Z; ch++)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey(ch)))
                    {
                        pBuffer->GetMode()->AddKeyPress((ch - KEY_A) + 'a', mod);
                        handled = true;
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey(KEY_SPACE)))
                {
                    pBuffer->GetMode()->AddKeyPress(' ', mod);
                    handled = true;
                }
            }
#else
            if (ImGui::IsKeyPressed(ImGuiKey('1')))
            {
                SetGlobalMode(ZepMode_Standard::StaticName());
                handled = true;
            }
            else if (ImGui::IsKeyPressed(ImGuiKey('2')))
            {
                SetGlobalMode(ZepMode_Vim::StaticName());
                handled = true;
            }
            else
            {
                for (int ch = '0'; ch <= '9'; ch++)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey(ch)))
                    {
                        pBuffer->GetMode()->AddKeyPress(ch, mod);
                        handled = true;
                    }
                }
                for (int ch = 'A'; ch <= 'Z'; ch++)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey(ch)))
                    {
                        pBuffer->GetMode()->AddKeyPress(ch - 'A' + 'a', mod);
                        handled = true;
                    }
                }

                if (ImGui::IsKeyPressed(ImGuiKey(KEY_SPACE)))
                {
                    pBuffer->GetMode()->AddKeyPress(' ', mod);
                    handled = true;
                }
            }
#endif
        }

        if (!handled)
        {
            for (int n = 0; n < io.InputQueueCharacters.Size && io.InputQueueCharacters[n]; n++)
            {
                // Ignore '\r' - sometimes ImGui generates it!
                if (io.InputQueueCharacters[n] == '\r')
                    continue;

                pBuffer->GetMode()->AddKeyPress(io.InputQueueCharacters[n], mod);
            }
        }
    }

private:
};

} // namespace Zep
