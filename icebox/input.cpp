#include "utils.h"

#include <imgui/imgui.h>

#include <sdl.h>

#include <animation/timer.h>

#include <jorvik/jorvik.h>
#include <jorvik/editor.h>

#include "input.h"

namespace Mgfx
{

Input input;

void input_init()
{
    for (auto& e : input.Keys)
    {
        e = InputState::None;
    }
    for (auto& t : input.touches)
    {
        t.state = InputState::None;
    }
}

void input_process_event(SDL_Event& event)
{
    if (!ImGui::GetIO().WantCaptureKeyboard &&
        !ImGui::GetIO().WantTextInput &&
        !ImGui::IsMouseHoveringAnyWindow() &&
        !ImGui::IsAnyItemFocused())
    {
        if (event.type == SDL_MOUSEBUTTONDOWN)
        {
            auto& record = input.touches[int((event.button.button == SDL_BUTTON_LEFT) ? TouchDeviceType::LeftMouse : TouchDeviceType::RightMouse)];
            SDL_GetMouseState(&record.downPos.x, &record.downPos.y);
            record.current = record.downPos;
            record.delta = glm::ivec2(0);
            record.state = InputState::Pressed;
        }
        else if (event.type == SDL_MOUSEBUTTONUP)
        {
            auto& record = input.touches[int((event.button.button == SDL_BUTTON_LEFT) ? TouchDeviceType::LeftMouse : TouchDeviceType::RightMouse)];
            SDL_GetMouseState(&record.upPos.x, &record.upPos.y);
            record.current = record.upPos;
            record.state = InputState::Released;
        }
        else if (event.type == SDL_MOUSEMOTION)
        {
            auto& record = input.touches[input.touches[int(TouchDeviceType::RightMouse)].state == InputState::Pressed ? int(TouchDeviceType::RightMouse) : int(TouchDeviceType::LeftMouse)];
            glm::ivec2 newPos;
            SDL_GetMouseState(&newPos.x, &newPos.y);
            record.delta = newPos - record.current;
            record.current = newPos;
            record.state = InputState::Moved;
        }
    }

    if (!ImGui::GetIO().WantCaptureKeyboard &&
        !ImGui::GetIO().WantTextInput &&
        !ImGui::IsMouseHoveringAnyWindow() &&
        !ImGui::IsAnyItemFocused())
    {
        auto key = event.key.keysym.scancode;
        auto state = event.key.state == SDL_PRESSED ? InputState::Pressed : InputState::Released;
        if (key == SDL_SCANCODE_ESCAPE)
        { 
            jorvik.done = true;
        }
        else if (key == SDL_SCANCODE_W)
        {
            input.Keys[int(GameKeyType::Forward)] = state;
        }
        else if (key == SDL_SCANCODE_S)
        {
            input.Keys[int(GameKeyType::Backward)] = state;
        }
        else if (key == SDL_SCANCODE_A)
        {
            input.Keys[int(GameKeyType::Left)] = state;
        }
        else if (key == SDL_SCANCODE_D)
        {
            input.Keys[int(GameKeyType::Right)] = state;
        }
        else if (key == SDL_SCANCODE_R)
        {
            input.Keys[int(GameKeyType::Up)] = state;
        }
        else if (key == SDL_SCANCODE_F)
        {
            input.Keys[int(GameKeyType::Down)] = state;
        }
    }
}


};
