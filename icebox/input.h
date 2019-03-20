#pragma once

#include <glm/glm.hpp>

union SDL_Event;

namespace Mgfx
{

enum class GameKeyType : uint32_t
{
    Forward,
    Backward,
    Right,
    Left,
    Up,
    Down,
    SaveLevel,
    Max
};

enum class InputState : uint32_t
{
    None,
    Pressed,
    Released,
    Moved
};

struct TouchRecord
{
    InputState state;
    glm::ivec2 downPos;
    glm::ivec2 upPos;
    glm::ivec2 delta;
    glm::ivec2 current;
};

enum class TouchDeviceType : uint32_t
{
    LeftMouse,
    RightMouse,
    Finger1,
    Max
};

struct Input
{
    InputState Keys[int(GameKeyType::Max)];
    TouchRecord touches[int(TouchDeviceType::Max)];
};
extern Input input;

void input_init();
void input_process_event(SDL_Event& event);

} // Mgfx