#pragma once
#include <string>

#include "zep/imgui/display_imgui.h"

#include "zep/editor.h"
#include "zep/mode_standard.h"
#include "zep/mode_vim.h"
#include "zep/syntax.h"
#include "zep/tab_window.h"
#include "zep/window.h"

namespace Zep
{

// These key defines from usb_hid_keys.h; standard USB keycodes.
// Defined here to stop collisions.

#define ZEP_KEY_F1 0x3a // Keyboard F1
#define ZEP_KEY_F2 0x3b // Keyboard F2
#define ZEP_KEY_F3 0x3c // Keyboard F3
#define ZEP_KEY_F4 0x3d // Keyboard F4
#define ZEP_KEY_F5 0x3e // Keyboard F5
#define ZEP_KEY_F6 0x3f // Keyboard F6
#define ZEP_KEY_F7 0x40 // Keyboard F7
#define ZEP_KEY_F8 0x41 // Keyboard F8
#define ZEP_KEY_F9 0x42 // Keyboard F9
#define ZEP_KEY_F10 0x43 // Keyboard F10
#define ZEP_KEY_F11 0x44 // Keyboard F11
#define ZEP_KEY_F12 0x45 // Keyboard F12

#define ZEP_KEY_1 0x1e // Keyboard 1 and !
#define ZEP_KEY_2 0x1f // Keyboard 2 and @
#define ZEP_KEY_3 0x20 // Keyboard 3 and #
#define ZEP_KEY_4 0x21 // Keyboard 4 and $
#define ZEP_KEY_5 0x22 // Keyboard 5 and %
#define ZEP_KEY_6 0x23 // Keyboard 6 and ^
#define ZEP_KEY_7 0x24 // Keyboard 7 and &
#define ZEP_KEY_8 0x25 // Keyboard 8 and *
#define ZEP_KEY_9 0x26 // Keyboard 9 and (
#define ZEP_KEY_0 0x27 // Keyboard 0 and )

#define ZEP_KEY_A 0x04 // Keyboard a and A
#define ZEP_KEY_Z 0x1d // Keyboard z and Z
#define ZEP_KEY_SPACE 0x2c // Keyboard Spacebar

class ZepDisplay_ImGui;
class ZepTabWindow;
class ZepEditor_ImGui : public ZepEditor
{
public:
    ZepEditor_ImGui(const ZepPath& root, const NVec2f& pixelScale, uint32_t flags = 0, IZepFileSystem* pFileSystem = nullptr)
        : ZepEditor(new ZepDisplay_ImGui(pixelScale), root, flags, pFileSystem)
    {
    }

    void HandleInput()
    {
        auto& io = ImGui::GetIO();

        bool handled = false;

        uint32_t mod = 0;

        static std::map<int, int> MapUSBKeys =
        {
            { ZEP_KEY_F1, ExtKeys::F1},
            { ZEP_KEY_F2, ExtKeys::F2},
            { ZEP_KEY_F3, ExtKeys::F3},
            { ZEP_KEY_F4, ExtKeys::F4},
            { ZEP_KEY_F5, ExtKeys::F5},
            { ZEP_KEY_F6, ExtKeys::F6},
            { ZEP_KEY_F7, ExtKeys::F7},
            { ZEP_KEY_F8, ExtKeys::F8},
            { ZEP_KEY_F9, ExtKeys::F9},
            { ZEP_KEY_F10, ExtKeys::F10},
            { ZEP_KEY_F11, ExtKeys::F11},
            { ZEP_KEY_F12, ExtKeys::F12}
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

        if (io.KeyCtrl)
        {
            mod |= ModifierKey::Ctrl;
        }
        if (io.KeyShift)
        {
            mod |= ModifierKey::Shift;
        }

        auto pWindow = GetActiveTabWindow()->GetActiveWindow();
        const auto& buffer = pWindow->GetBuffer();

        // Check USB Keys
        for (auto& usbKey : MapUSBKeys)
        {
            if (ImGui::IsKeyPressed(usbKey.first))
            {
                buffer.GetMode()->AddKeyPress(usbKey.second, mod);
                return;
            }
        }

        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::TAB, mod);
            return;
        }
        if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::ESCAPE, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::RETURN, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::DEL, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::HOME, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::END, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::BACKSPACE, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::RIGHT, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::LEFT, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::UP, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::DOWN, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::PAGEDOWN, mod);
            return;
        }
        else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp)))
        {
            buffer.GetMode()->AddKeyPress(ExtKeys::PAGEUP, mod);
            return;
        }
        else if (io.KeyCtrl)
        {
            // SDL Remaps to its own scancodes; and since we can't look them up in the standard IMGui list
            // without modifying the ImGui base code, we have special handling here for CTRL.
            // For the Win32 case, we use VK_A (ASCII) is handled below
#if defined(_SDL_H) || defined(ZEP_USE_SDL)
            if (ImGui::IsKeyPressed(ZEP_KEY_1))
            {
                SetGlobalMode(ZepMode_Standard::StaticName());
                handled = true;
            }
            else if (ImGui::IsKeyPressed(ZEP_KEY_2))
            {
                SetGlobalMode(ZepMode_Vim::StaticName());
                handled = true;
            }
            else
            {
                for (int ch = ZEP_KEY_A; ch <= ZEP_KEY_Z; ch++)
                {
                    if (ImGui::IsKeyPressed(ch))
                    {
                        buffer.GetMode()->AddKeyPress((ch - ZEP_KEY_A) + 'a', mod);
                        handled = true;
                    }
                }

                if (ImGui::IsKeyPressed(ZEP_KEY_SPACE))
                {
                    buffer.GetMode()->AddKeyPress(' ', mod);
                    handled = true;
                }
            }
#else
            if (ImGui::IsKeyPressed('1'))
            {
                SetGlobalMode(ZepMode_Standard::StaticName());
                handled = true;
            }
            else if (ImGui::IsKeyPressed('2'))
            {
                SetGlobalMode(ZepMode_Vim::StaticName());
                handled = true;
            }
            else
            {
                for (int ch = 'A'; ch <= 'Z'; ch++)
                {
                    if (ImGui::IsKeyPressed(ch))
                    {
                        buffer.GetMode()->AddKeyPress(ch - 'A' + 'a', mod);
                        handled = true;
                    }
                }

                if (ImGui::IsKeyPressed(ZEP_KEY_SPACE))
                {
                    buffer.GetMode()->AddKeyPress(' ', mod);
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

                buffer.GetMode()->AddKeyPress(io.InputQueueCharacters[n], mod);
            }
        }
    }

private:
};

} // namespace Zep
