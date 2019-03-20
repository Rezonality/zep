#include "utils.h"

#include <imgui/imgui.h>
#include "material.h"
#include "entities/entities.h"
#include "file/file.h"

namespace Mgfx
{


void materials_init()
{
}

void materials_destroy()
{
//  TODO   entity_destroy(defaultMaterial);
}

Entity* material_get_default()
{
    // Note: Entity not in the level, it is 'global'
    auto defaultMaterial = entity_create(0, "default_material");
    auto pMat = component_add<MaterialComponent>(defaultMaterial);
    pMat->name = "Default";
    return defaultMaterial;
}

void MaterialComponent::Destroy()
{
    if (deviceMaterial != nullptr)
    {
        free(deviceMaterial);
        deviceMaterial = nullptr;
    }
}

bool MaterialComponent::ShowUI()
{
    bool changed = false;
    if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::LabelText("Name", name.c_str());
        if (ImGui::ColorEdit4("Base Color", &pbrMetallic.baseColorFactor[0]))
            changed = true;
        if (ImGui::SliderFloat("Metallic", &pbrMetallic.metallicFactor, 0.0f, 1.0f))
            changed = true;
        if (ImGui::SliderFloat("Roughness", &pbrMetallic.roughnessFactor, 0.0f, 1.0f))
            changed = true;
    }
    return changed;
}

} // namespace Mgfx
