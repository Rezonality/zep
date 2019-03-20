#pragma once

#include "IDevice.h"

struct SDL_Window;
union SDL_Event;

namespace Mgfx
{

class Camera;
class Mesh;
class Scene;

class Window;
struct WindowData;

class DeviceNull : public IDevice
{
public:
    DeviceNull();
    ~DeviceNull();
    virtual bool Init(const char* pszWindowName) override;

    virtual bool Begin3D(const std::shared_ptr<Scene>& spScene) override;
    virtual void End3D() override;
    virtual void DrawMesh(Mesh* pMesh, GeometryType type) override;

    // 2D Rendering functions
    virtual bool Begin2D(const glm::vec4& clearColor) override;
    uint32_t CreateTexture() override;
    virtual void DestroyTexture(uint32_t id) override;
    virtual glm::u8vec4* ResizeTexture(uint32_t id, const glm::uvec2& size) override;
    virtual void UpdateTexture(uint32_t id) override;
    virtual void DrawTexture(uint32_t id, const std::vector<GeometryVertex>& vertices) override;
    virtual void End2D() override;

    virtual void BeginGUI() override;
    virtual void EndGUI() override;
    virtual void Cleanup() override;
    virtual void ProcessEvent(SDL_Event& event) override;
    virtual void Flush() override;
    virtual void Swap() override;
    virtual SDL_Window* GetSDLWindow() const override { return pSDLWindow; }
    virtual const char* GetName() const { return "Null Device"; }

private:
    SDL_Window* pSDLWindow = nullptr;
    unsigned int m_currentTextureID = 1;
    std::vector<glm::u8vec4> m_quadData;
};

} // Mgfx namespace
