#include "utils.h"
#include "utils/logger.h"

#include <imgui/imgui.h>

#include <glm/packing.hpp>


#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"

#include "ui/ui_manager.h"

#include "jorvik.h"
#include "material.h"
#include "mesh.h"
#include "editor.h"

#include "jorvik/device/dx12/device_dx12.h"
#include "utils/string/murmur_hash.h"

#undef ERROR
namespace Mgfx
{

void meshes_destroy()
{
    //LOG(INFO) << "Meshes left at destroy: " << component_count(MeshComponent::CID);
}

float MeshComponent::GetMaxSize()
{
    return glm::length(boundsMax - boundsMin);
}

glm::vec3 MeshComponent::GetCenter()
{
    return (boundsMax + boundsMin) * .5f;
}

glm::vec3 MeshComponent::GetCenterBottom()
{
    return glm::vec3(GetCenter().x, boundsMin.y, GetCenter().z);
}

void MeshComponent::CreateBox(const glm::vec3& sz, const glm::vec3& off)
{
    meshType = MeshType::Box;
    size = sz;
    offset = off;

    static const float cubeVertices[] = { -1.0f, -1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, +1.0f, -1.0f, +1.0f, +1.0f, +1.0f, -1.0f, -1.0f, +1.0f,
        -1.0f, +1.0f, -1.0f, +1.0f, -1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, -1.0f, +1.0f,
        -1.0f, -1.0f, -1.0f, +1.0f, -1.0f, +1.0f, -1.0f, +1.0f, +1.0f, +1.0f, -1.0f, +1.0f, +1.0f, -1.0f, -1.0f, +1.0f,
        -1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, -1.0f, +1.0f, +1.0f,
        -1.0f, -1.0f, -1.0f, +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f, +1.0f, +1.0f, -1.0f, +1.0f, -1.0f, +1.0f,
        +1.0f, -1.0f, -1.0f, +1.0f, +1.0f, -1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, +1.0f, -1.0f, +1.0f };

    static const float cubeNormals[] = { 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f,
        0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f,
        0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f,
        -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f };

    static const float cubeTangents[] = { +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f,
        +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f,
        -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
        +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f, 0.0f, 0.0f, +1.0f,
        0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -1.0f };

    static const float cubeTexCoords[] = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f };

    static const uint16_t cubeIndices[] = { 0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 15, 14, 12, 14, 13, 16, 17, 18, 16, 18, 19, 20, 23, 22, 20, 22, 21 };

    constexpr uint32_t numberVertices = sizeof(cubeVertices) / (sizeof(float) * 3);
    constexpr uint32_t numberIndices = sizeof(cubeIndices) / sizeof(uint16_t);

    std::vector<MeshVertex> vertices;
    std::vector<uint16_t> indices;
    vertices.resize(numberVertices);
    indices.resize(numberIndices);

    for (uint32_t i = 0; i < numberVertices; i++)
    {
        memcpy(&vertices[i].position, &cubeVertices[i * 4], sizeof(glm::vec3));
        memcpy(&vertices[i].normal, &cubeNormals[i * 3], sizeof(glm::vec3));
        memcpy(&vertices[i].texcoord, &cubeTexCoords[i * 2], sizeof(glm::vec2));
        memcpy(&vertices[i].tangent, &cubeTangents[i * 3], sizeof(glm::vec2));
        vertices[i].binormal = glm::normalize(glm::cross(vertices[i].normal, vertices[i].tangent));
    }

    for (uint32_t i = 0; i < numberIndices; i++)
    {
        memcpy(&indices[i], &cubeIndices[i], sizeof(uint16_t));
    }

    for (uint32_t i = 0; i < numberVertices; i++)
    {
        vertices[i].position *= size * .5f;
        vertices[i].position += offset;
    }

    boundsMin = glm::vec3(-size * .5f);
    boundsMax = glm::vec3(size * .5f);
    boundsMin += offset;
    boundsMax += offset;

    meshPath.clear();

    // Create a single part for the whole mesh, using the standard material
    MeshPart part;
    part.IndexBase = 0;
    part.NumIndices = numberIndices;
    part.Material = material_get_default();
    part.PartName = "CreatedMesh";
    parts.push_back(part);

    //deviceMesh = jorvik.spDevice->CreateMesh(this, numberVertices, numberIndices, &vertices[0], &indices[0]);
}

bool MeshComponent::ShowUI()
{
    static int selectedPart = 0;

    bool changed = false;
    if (ImGui::CollapsingHeader("Mesh Component", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!parts.empty())
        {
            if (selectedPart >= parts.size())
            {
                selectedPart = int(parts.size() - 1);
            }
  
            // Find all the materials
            std::vector<std::string> materials;
            for (uint32_t index = 0; index < parts.size(); index++)
            {
                auto matId = GetPartMaterial(index);
                materials.push_back(component_get<MaterialComponent>(matId)->name);
            }
            ImGui::Combo("Materials", &selectedPart, materials);
            
            auto matId = GetPartMaterial(selectedPart);
            ASSERT_COMPONENT(matId, MaterialComponent);
            auto pMat = component_get<MaterialComponent>(matId);
            if (pMat)
            {
                pMat->ShowUI();
            }
        }
    }
    return changed;
}

void MeshComponent::Destroy()
{
    if (deviceMesh != nullptr)
    {
        // TOOD: This reset is required because the component memory is allocated/freed without constructor
        //jorvik.spDevice->DestroyMesh(deviceMesh.get());
        deviceMesh.reset();
    }
}

Entity* MeshComponent::GetPartMaterial(uint32_t part)
{
    auto itr = materialOverrides.find(part);
    if (itr != materialOverrides.end())
    {
        return itr->second;
    }

    if (parts.size() > part)
    {
        return parts[part].Material;
    }

    return 0;
}

void MeshComponent::Load(const fs::path& path)
{
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    LOG(INFO) << "Loading: " << path;
    this->meshPath = path;
    bool ret = false;
    if (path.has_extension() && path.extension().string() == ".glb")
    {
        // assume binary glTF.
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string().c_str());
    }
    else
    {
        // assume ascii glTF.
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string().c_str());
    }

    if (!warn.empty())
    {
        LOG(INFO) << "GLTF WARNING: " << warn;
    }

    if (!ret)
    {
        LOG(INFO) << "GLTF: Failed to load: " << err;
        return;
    }

    if (!err.empty())
    {
        LOG(INFO) << "GLTF ERROR: " << err;
        return;
    }

    // Print model info
    LOG(INFO) << "Loaded GLTF: " << path;
    LOG(INFO) << "Materials: " << model.materials.size();
    LOG(INFO) << "Scenes: " << model.scenes.size();
    LOG(INFO) << "Animations: " << model.animations.size();
    LOG(INFO) << "Cameras: " << model.cameras.size();
    LOG(INFO) << "Textures: " << model.textures.size();
    LOG(INFO) << "Lights: " << model.lights.size();
    LOG(INFO) << "Buffers: " << model.buffers.size();
    LOG(INFO) << "Buffer Views: " << model.bufferViews.size();
    LOG(INFO) << "Images: " << model.images.size();
    LOG(INFO) << "Samplers: " << model.samplers.size();
    LOG(INFO) << "Skins: " << model.skins.size();
    LOG(INFO) << "Meshes: " << model.meshes.size();

    LOG(INFO) << "Nodes:" << model.nodes.size();
    std::ostringstream str;
    for (auto& node : model.nodes)
    {
        str << node.name << ", ";
    }
    LOG(INFO) << str.str();

    LOG(INFO) << "Extensions: " << model.extensions.size();
    if (!model.extensions.empty())
    {
        for (auto& ex : model.extensions)
        {
            LOG(INFO) << ex.first;
        }
    }

    LOG(WARNING) << "Haven't written model loader yet!";
    meshType = MeshType::File;
    /*
    m_boundsMin.x = pModel->BoundingBox()->min().x();
    m_boundsMin.y = pModel->BoundingBox()->min().y();
    m_boundsMin.z = pModel->BoundingBox()->min().z();

    m_boundsMax.x = pModel->BoundingBox()->max().x();
    m_boundsMax.y = pModel->BoundingBox()->max().y();
    m_boundsMax.z = pModel->BoundingBox()->max().z();
    */

    /*
    auto pMeshData = GetData();
    for (uint32_t i = 0; i < pModel->Meshes()->Length(); i++)
    {
        auto spPart = std::make_shared<MeshPart>(&mesh);
        auto pMesh = pModel->Meshes()->Get(i);

        spPart->Positions.resize(pMesh->Vertices()->Length() / 3);
        memcpy(&spPart->Positions[0], pMesh->Vertices()->data(), pMesh->Vertices()->Length() * sizeof(float));

        spPart->Normals.resize(pMesh->Normals()->Length() / 3);
        memcpy(&spPart->Normals[0], pMesh->Normals()->data(), pMesh->Normals()->Length() * sizeof(float));

        spPart->Indices.resize(pMesh->Indices()->Length());
        memcpy(&spPart->Indices[0], pMesh->Indices()->data(), pMesh->Indices()->Length() * sizeof(float));

        if (pMesh->Colors0() && pMesh->Colors0()->Length() != 0)
        {
            spPart->Diffuse.clear();
            for (uint32_t index = 0; index < pMesh->Colors0()->Length(); index++)
            {
                auto& col = pMesh->Colors0()->data()[index];
                spPart->Diffuse.push_back(glm::unpackUnorm4x8(col));
            }
        }
        else
        {
            spPart->Diffuse.resize(spPart->Positions.size());
            std::fill(spPart->Diffuse.begin(), spPart->Diffuse.end(), glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }

        if (pMesh->TexCoords0() && pMesh->TexCoords0()->Length() > 0)
        {
            spPart->UVs.resize(pMesh->TexCoords0()->Length() / 2);
            memcpy(&spPart->UVs[0], pMesh->TexCoords0()->data(), pMesh->TexCoords0()->Length() * sizeof(float));
        }
        else
        {
            spPart->UVs.resize(spPart->Positions.size());
            std::fill(spPart->UVs.begin(), spPart->UVs.end(), glm::vec3(0.0f, 0.0f, 0.0f));
        }

        spPart->MaterialID = pMesh->MaterialIndex();

        pMeshData->meshParts.push_back(spPart);
    }

    for (uint32_t i = 0; i < pModel->Materials()->Length(); i++)
    {
        auto spMat = std::make_shared<Material>();
        auto pMaterial = pModel->Materials()->Get(i);

        spMat->name = pMaterial->Name()->str();

        auto ToGLM3 = [](const MGeo::vec3* v) { return glm::vec3(v->x(), v->y(), v->z()); };
        auto ToGLM4 = [](const MGeo::vec4* v) { return glm::vec4(v->x(), v->y(), v->z(), v->w()); };

        spMat->globalDiffuse = ToGLM3(pMaterial->Diffuse());
        spMat->specular = ToGLM4(pMaterial->Specular());
        spMat->ambient = ToGLM3(pMaterial->Ambient());
        spMat->emissive = ToGLM3(pMaterial->Emissive());
        spMat->transparent = ToGLM3(pMaterial->Transparent());

        spMat->opacity = pMaterial->Opacity();
        spMat->specularStrength = pMaterial->SpecularStrength();

        spMat->diffuseTex = pMaterial->DiffuseTex()->str();
        spMat->specularTex = pMaterial->SpecularTex()->str();
        spMat->normalTex = pMaterial->NormalTex()->str();
        spMat->emissiveTex = pMaterial->EmissiveTex()->str();
        spMat->lightMapTex = pMaterial->LightMapTex()->str();
        spMat->reflectionTex = pMaterial->ReflectionTex()->str();
        spMat->heightTex = pMaterial->HeightTex()->str();
        spMat->shinyTex = pMaterial->ShinyTex()->str();
        spMat->ambientTex = pMaterial->AmbientTex()->str();
        spMat->displacementTex = pMaterial->DisplacementTex()->str();

        pMeshData->materials.push_back(spMat);
    }
    */
}

/*
bool mesh_load(MeshComponent& mesh, const fs::path& modelPath)
{
    auto pModel = MGeo::GetModel(&data[0]);
}*/
} // namespace Mgfx
