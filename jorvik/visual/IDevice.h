#pragma once

#include <SDL.h>
#include <future>
#include <glm/glm.hpp>

#include "file/file.h"
#include "meta_tags.h"

namespace Mgfx
{

struct PassState;
class CompileResult;

// Provides an interface for an application that owns DxDeviceResources to be notified of the device being lost or created.
struct IDeviceNotify
{
    virtual void OnDeviceLost() = 0;
    virtual void OnDeviceRestored() = 0;
    virtual void OnInvalidateDeviceObjects() = 0;
    virtual void OnCreateDeviceObjects() = 0;
    virtual void OnBeginResize() = 0;
    virtual void OnEndResize() = 0;
};

struct IDevice : public IDeviceNotify
{
    // Device methods, typically called in this order
    virtual bool Init(const char* pszWindowName) = 0;
    virtual void ProcessEvent(SDL_Event& event) = 0;
    virtual bool RenderFrame(float frameDelta, std::function<void()> fnRenderObjects) = 0;
    virtual void BeginGUI() = 0;
    virtual void EndGUI() = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void Destroy() = 0;
    virtual void Wait() = 0;

    // Temporary quad interface before I add more complex geometry methods and VS/Geometry matching code
    virtual void DrawFSQuad(std::shared_ptr<CompileResult> state) = 0;

    // Helpers
    virtual const char* GetName() = 0;
    virtual glm::uvec2 GetWindowSize() = 0;

    virtual std::future<std::shared_ptr<CompileResult>> CompileShader(const fs::path& path, const std::string& strText) = 0;
    virtual std::future<std::shared_ptr<CompileResult>> CompilePass(PassState* spPassRenderState) = 0;

    glm::vec4 clearColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    SDL_Window* pWindow = nullptr;
    bool Initialized = false;
    bool enable_vsync = false;
    bool enable_fxaa = false;
};

}; // namespace Mgfx
