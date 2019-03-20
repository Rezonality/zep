#include "utils.h"

#include <imgui.h>

#include <animation/timer.h>

#include "input.h"
#include "control.h"
#include "visual/body.h"

namespace Mgfx
{

bool ControllableComponent::ShowUI()
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Controllable Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Checkbox("Active", &active))
            changed = true;
        if (ImGui::DragFloat("Acceleration", &acceleration, 1.0f, 0.0f, 100.0f))
            changed = true;
        if (ImGui::DragFloat("Max Acceleration", &max_acceleration, 1.0f, 0.0f, 800.0f))
            changed = true;
    }
    return changed;
}

void control_components_move(TimeDelta dt)
{
    auto list = entities_with_components(ControllableComponent::CID | BodyComponent::CID);
    for (auto entity : list)
    {
        if (entity)
        {
            auto controllable = component_get<ControllableComponent>(entity);
            auto position = component_get<BodyComponent>(entity);
            if (controllable->active)
            {
                auto dir = glm::normalize(position->orientation * glm::vec3(0.0f, 0.0f, -1.0f));

                // Apply acceleration to the entity
                if (input.Keys[int(GameKeyType::Forward)] == InputState::Pressed)
                {
                    position->acceleration += (dir * controllable->acceleration);
                }
                else if (input.Keys[int(GameKeyType::Backward)] == InputState::Pressed)
                {
                    position->acceleration += (-dir * controllable->acceleration);
                }

                position->acceleration = glm::min(position->acceleration, glm::vec3(controllable->max_acceleration));
                position->acceleration = glm::max(position->acceleration, glm::vec3(-controllable->max_acceleration));
                continue;
            }
        }
    }
};

} // Mgfx
