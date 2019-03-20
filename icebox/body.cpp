#include "utils.h"

#include <imgui/imgui.h>

#include "body.h"

namespace Mgfx
{

bool BodyComponent::ShowUI()
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Body Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::DragFloat3("Position", &position[0], 1.0f))
            changed = true;
        if (ImGui::DragFloat3("Orientation", &orientation[0], 1.0f))
            changed = true;
        if (ImGui::DragFloat3("Acceleration", &acceleration[0], 1.0f))
            changed = true;
        if (ImGui::DragFloat("Attrition", &attrition, 1.0f, 0.0f, 1.0f))
            changed = true;

        if (ImGui::DragFloat3("Velocity", &velocity[0], 1.0f, 0.0f, 100.0f))
            changed = true;
        if (ImGui::DragFloat("Max Velocity", &max_velocity, 1.0f, 0.0f, 100.0f))
            changed = true;

        if (ImGui::DragFloat3("Angular Velocity", &angular_velocity[0], 1.0f, 0.0f, 100.0f))
            changed = true;
        if (ImGui::DragFloat("Max Angular Velocity", &max_velocity, 1.0f, 0.0f, 100.0f))
            changed = true;
        if (ImGui::DragFloat("Mass", &mass, 1.0f, 0.0f, 1000.0f))
            changed = true;
    }
    return changed;
}

}