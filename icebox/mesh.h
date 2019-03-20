#pragma once

#include "entities/entities.h"
#include "file/file.h"
#include "material.h"
#include <map>
#include "device/IDevice.h"

namespace Mgfx
{

// Mesh part, part of the mesh structure owned by the device
struct MeshPart
{
    std::string PartName;
    uint32_t IndexBase = 0;
    uint32_t VertexBase = 0;
    uint32_t NumIndices = 0;

    // Material used by this part
    // TODO: Should this be an entity?
    // Make the UI reflect a generic Interface.
    // Then make materials some generic thing.
    // Get away from components!
    Entity* Material = nullptr;
};

enum MeshType : uint32_t
{
    File,
    Box
};


struct DeviceMesh;
struct MeshComponent : public Component
{
    COMPONENT(MeshComponent);

    // Thing type
    fs::path meshPath;
    MeshType meshType = MeshType::Box;

    // Bounds
    glm::vec3 boundsMin = glm::vec3(-1.0f);
    glm::vec3 boundsMax = glm::vec3(1.0f);
 
    // Location
    glm::vec3 size = glm::vec3(1.0f);
    glm::vec3 offset = glm::vec3(0.0f);

    // Override materials, if the user has tweaked them
    std::map<uint32_t, Entity*> materialOverrides;

    // A list of mesh parts
    std::vector<MeshPart> parts;

    // Created for the mesh
    std::shared_ptr<DeviceMesh> deviceMesh;

    virtual bool ShowUI() override;
    virtual void Destroy() override;

    // Component methods
    virtual void Load(const fs::path& meshPath);
    virtual void CreateBox(const glm::vec3& size, const glm::vec3& offset);
    virtual float GetMaxSize();
    virtual glm::vec3 GetCenter();
    virtual glm::vec3 GetCenterBottom();
    virtual Entity* GetPartMaterial(uint32_t part);
};

void meshes_destroy();

} // namespace Mgfx
