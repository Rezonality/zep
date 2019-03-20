#include "mcommon.h"

#include "device/Null/deviceNull.h"
#include "camera/camera.h"
#include "scene/scene.h"
#include "geometry/mesh.h"
#include "ui/windowmanager.h"
#include "ui/window.h"
#include "file/media_manager.h"
#include <iostream>

namespace Mgfx
{

DeviceNull::DeviceNull()
{

}

DeviceNull::~DeviceNull()
{

}

bool DeviceNull::Init(const char* pszWindowName)
{
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    pSDLWindow = SDL_CreateWindow(pszWindowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    return true;
}

bool DeviceNull::Begin2D(const glm::vec4& clearColor)
{
    return true;
}

uint32_t DeviceNull::CreateQuad()
{
    return m_currentQuadID++;
}

void DeviceNull::DestroyQuad(uint32_t id)
{
}

glm::u8vec4* DeviceNull::ResizeQuad(uint32_t id, const glm::uvec2& size)
{
    m_quadData.resize(size.x * size.y * sizeof(glm::u8vec4));
    return &m_quadData[0];
}

void DeviceNull::UpdateQuad(uint32_t id)
{

}

void DeviceNull::DrawQuad(uint32_t id, const std::vector<QuadVertex>& vertices)
{
}

void DeviceNull::End2D()
{
}

bool DeviceNull::Begin3D(const std::shared_ptr<Scene>& spScene)
{
    return true;
}

void DeviceNull::End3D()
{
}

void DeviceNull::Draw(Mesh* pMesh, GeometryType type)
{
}

void DeviceNull::Cleanup()
{
}

void DeviceNull::BeginGUI()
{
//    ImGui::NewFrame();
}

void DeviceNull::EndGUI()
{
}

void DeviceNull::ProcessEvent(SDL_Event& event)
{
}

void DeviceNull::Swap()
{
}

} // namespace Mgfx
