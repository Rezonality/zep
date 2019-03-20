#pragma once

#include <glm/glm.hpp>
#include "entities/entities.h"

namespace Mgfx
{

struct PhysicsComponent : Component
{
    COMPONENT(PhysicsComponent)

    virtual bool ShowUI() override;

    glm::vec3 momentum = glm::vec3(0.0);
};

void physics_tick(TimeDelta dt);
void physics_init();

}
