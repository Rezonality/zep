#pragma once

#include "entities/entities.h"

namespace Mgfx
{

struct ControllableComponent : Component
{
    COMPONENT(ControllableComponent);

    // How much acceleration this body has as a result of control inputs
    // Some things accelerate faster than others
    bool active = true;
    float acceleration = 100.0f;          // m/s/s
    float max_acceleration = 800.0f;      // m/s/s
    
    virtual bool ShowUI() override;
};

void control_components_move(TimeDelta dt);

}