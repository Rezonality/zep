#pragma once

#include <glm/glm.hpp>
#include "entities/entities.h"
#include "file/file.h"

namespace Mgfx
{

struct DeviceMaterial;
struct DeviceTexture;

struct Texture
{
    fs::path filePath;

    // Which tex coord set to use in the mesh
    uint32_t coordinateSet = 0;

    // Scale of texture data, for normals
    float scale = 1.0f;

    // Device-mapped version of the texture
    DeviceTexture* pDeviceTexture = nullptr;
};

struct PBRMetallicRoughness
{
    glm::vec4 baseColorFactor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0);
   
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    
    Texture metallicRoughnessTexture;
    Texture baseColorTexture;
};

enum class MaterialAlphaMode
{
    Opaque = 0,
    Mask = 1,
    Blend = 2
};

struct MaterialComponent : public Component
{
    COMPONENT(MaterialComponent);

    virtual void Destroy() override;

    std::string name;

    // Material lighting model
    PBRMetallicRoughness pbrMetallic;

    // Additional textures
    Texture normalTexture;
    Texture occlussionTexture;
    Texture emissiveTexture;

    glm::vec3 emissiveFactor = glm::vec3(0.0f, 0.0f, 0.0f);

    MaterialAlphaMode alphaMode;
    float alphaCutoff = 0.5f;

    bool doubleSided = false;

    // Device
    DeviceMaterial* deviceMaterial;

    virtual bool ShowUI() override;
}; 

Entity* material_get_default();
void materials_init();
void materials_destroy();

} // namespace Mgfx
