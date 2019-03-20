#include "utils.h"
#include "camera_manipulator.h"

namespace Mgfx
{

/*
// This simple manipulator processes events and moves a camera.
void CameraManipulator::ProcessEvent(SDL_Event& event)
{
    if (event.type == SDL_MOUSEBUTTONDOWN)
    {
        // If the left button was pressed. 
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            int x, y;
            SDL_GetMouseState(&x, &y);
            MouseDown(glm::vec2(x, y));
        }
    }
    else if (event.type == SDL_MOUSEBUTTONUP)
    {
        // If the left button was pressed. 
        if (event.button.button == SDL_BUTTON_LEFT)
        {
            int x, y;
            SDL_GetMouseState(&x, &y);
            MouseUp(glm::vec2(x, y));
        }
    }
    else if (event.type == SDL_MOUSEMOTION)
    {
        int x, y;
        SDL_GetMouseState(&x, &y);
        currentPos = glm::vec2(x, y);

    }
    else if (event.type == SDL_WINDOWEVENT)
    {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
        {
            spCamera->SetFilmSize(glm::uvec2(event.window.data1, event.window.data2));
        }
    }
    else if (event.type == SDL_KEYDOWN ||
        event.type == SDL_KEYUP)
    {
        ProcessKeyboard(event);
    }
}

void CameraManipulator::ProcessKeyboard(SDL_Event& event)
{
    //auto keyState = SDL_GetKeyboardState(nullptr);

    if (ImGui::GetIO().WantCaptureKeyboard ||
        ImGui::GetIO().WantTextInput)
    {
        walkDirection = glm::vec3(0.0f);
        return;
    }

    auto key = event.key.keysym.scancode;
    if (key == SDL_SCANCODE_W)
    {
        walkDirection.z = event.key.state == SDL_PRESSED ? 1.0f : 0.0f;
    }
    else if (key == SDL_SCANCODE_S)
    {
        walkDirection.z = event.key.state == SDL_PRESSED ? -1.0f : 0.0f;
    }
    else if (key == SDL_SCANCODE_A)
    {
        walkDirection.x = event.key.state == SDL_PRESSED ? -1.0f : 0.0f;
    }
    else if (key == SDL_SCANCODE_D)
    {
        walkDirection.x = event.key.state == SDL_PRESSED ? 1.0f : 0.0f;
    }
    else if (key == SDL_SCANCODE_R)
    {
        walkDirection.y = event.key.state == SDL_PRESSED ? 1.0f : 0.0f;
    }
    else if (key == SDL_SCANCODE_F)
    {
        walkDirection.y = event.key.state == SDL_PRESSED ? -1.0f : 0.0f;
    }
}

bool CameraManipulator::MouseMove(const glm::vec2& delta)
{
    auto keyState = SDL_GetKeyboardState(nullptr);
    if (mouseDown)
    {
        if (keyState[SDL_SCANCODE_LCTRL])
        {
            spCamera->Dolly(delta.y / 4.0f);
        }
        else
        {
            spCamera->Orbit(delta / 2.0f);
        }
    }
    return true;
}

void CameraManipulator::Update(float deltaTime)
{
    m_currentDelta += deltaTime;
    if (m_currentDelta > .01f)
    {
        orbitDelta = startPos - currentPos;
        if (orbitDelta != glm::vec2(0.0f))
        {
            orbitDelta *= (m_currentDelta * 40.0f);
            MouseMove(orbitDelta);

            orbitDelta = glm::vec2(0.0f);
            startPos = currentPos;
        }

        if (walkDirection != glm::vec3(0.0f))
        {
            spCamera->Walk(walkDirection * (m_currentDelta * 400.0f));
        }
        m_currentDelta = 0.0f;
    }
}
*/
} // namespace Mgfx
