#pragma once

namespace Mgfx
{

/*
class CameraManipulator : public IWindowManipulator
{
private:
    glm::vec2 startPos = glm::vec2(0.0f);
    glm::vec2 currentPos = glm::vec2(0.0f);
    
    glm::vec3 walkDirection = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec2 orbitDelta = glm::vec2(0.0f);

    std::shared_ptr<Camera> spCamera;
    bool mouseDown;

    float m_currentDelta = 0.0f;
public:
    CameraManipulator(std::shared_ptr<Camera> pCam)
        : spCamera(pCam),
        mouseDown(false)
    {

    }

    void MouseDown(const glm::vec2& pos)
    {
        startPos = pos;
        mouseDown = true;
    }

    void MouseUp(const glm::vec2& pos)
    {
        currentPos = pos;
        mouseDown = false;
    }

    bool MouseMove(const glm::vec2& pos);

    void ProcessKeyboard(SDL_Event& ev);

    void ProcessEvent(SDL_Event& ev) override;
    void Update(float deltaTime) override;
};
*/

} // namespace Mgfx
