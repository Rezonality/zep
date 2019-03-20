#include "utils.h"

#include "physics.h"
#include "body.h"

#include <imgui/imgui.h>

namespace Mgfx
{

void physics_init()
{

}

bool PhysicsComponent::ShowUI()
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Physics Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::DragFloat3("Momentum", &momentum[0], 1.0f))
            changed = true;
    }
    return changed;
}

void physics_tick(TimeDelta dt)
{
    auto list = entities_with_components(BodyComponent::CID);
    for (auto entity : list)
    {
        if (entity)
        {
            auto position = component_get<BodyComponent>(entity);

            position->velocity *= position->attrition;

            // Velocity
            position->velocity += position->acceleration * dt;

            // TODO: Clamp max velocity correctly
            glm::clamp(position->velocity, glm::vec3(-position->max_velocity), glm::vec3(position->max_velocity));

            // Position
            position->position += position->velocity * dt;

            // Reset acceleration
            position->acceleration = glm::vec3(0.0f);
        }
    }
};

}
