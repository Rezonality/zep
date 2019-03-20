#pragma once

#include "glm/glm/gtc/quaternion.hpp"
#include "entities/entities.h"

namespace Mgfx
{

struct BodyComponent : Component
{
    COMPONENT(BodyComponent);

    glm::vec3 position = glm::vec3(0.0f);
    glm::quat orientation = glm::quat();
    glm::vec3 acceleration = glm::vec3(0.0f);
    float attrition = 0.9f;

    glm::vec3 velocity = glm::vec3(0.0f);
    float max_velocity = .75f;
    glm::vec3 angular_velocity = glm::vec3(0.0f);
    float max_angular_velocity = 1.0f;
    float mass = 1.0f;

    float distance(BodyComponent& other)
    {
        return glm::distance(position, other.position);
    }

    virtual bool ShowUI() override;
};

} // Mgfx
