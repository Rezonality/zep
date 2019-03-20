#pragma once

#include <glm/glm.hpp>

#include "utils/logger.h"
#include "compile.h"
#include "visual/IDevice.h"
#include "vulkan/vulkan.h"

struct SDL_Window;
union SDL_Event;

namespace Mgfx
{

class DeviceVulkan;
struct PassState;
struct ConstantsVulkan;

class CompiledShaderAssetVulkan : public CompileResult
{
public:
    virtual ~CompiledShaderAssetVulkan()
    {
        LOG(DEBUG) << "Destroyed a ShaderAssetVulkan";
    }
    //std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDesc;
    //Microsoft::WRL::ComPtr<ID3D10Blob> pShader;
    std::vector<std::string> semantics;
    std::shared_ptr<ConstantsVulkan> spConstants;
};

struct CompiledPassRenderStateAssetVulkan : CompileResult
{
    virtual ~CompiledPassRenderStateAssetVulkan()
    {
        LOG(DEBUG) << "Destroyed a PassRenderStateAssetVulkan";
    }

    //Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState;
    //RootSignature rootSignature;
    std::shared_ptr<ConstantsVulkan> spConstants;
};

class DeviceVulkan : public IDevice
{
public:
    DeviceVulkan();
    ~DeviceVulkan();

    // Vulkan methods
    virtual void CreateInstance();

    virtual bool Init(const char* pszWindowName) override;
    virtual void Destroy() override;
    virtual bool RenderFrame(float frameDelta, std::function<void()> fnRenderObjects) override;
    virtual void BeginGUI() override;
    virtual void EndGUI() override;
    virtual void Resize(int width, int height) override;
    virtual void ProcessEvent(SDL_Event& event) override;
    virtual const char* GetName() override;
    virtual glm::uvec2 GetWindowSize() override;
    virtual void Wait() override;

    virtual std::future<std::shared_ptr<CompileResult>> CompileShader(const fs::path& path, const std::string& strText) override;
    virtual std::future<std::shared_ptr<CompileResult>> CompilePass(PassState* pPassRenderState) override;

    virtual void DrawFSQuad(std::shared_ptr<CompileResult>& state) override;

    // Inherited via IDeviceNotify
    //virtual void OnDeviceLost() override;
    //virtual void OnDeviceRestored() override;

    /*DX::DeviceResources& GetDeviceResources() const
    {
        return *m_deviceResources;
    }*/

private:
    void Clear();

private:
 
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    size_t currentFrame = 0;
};

} // namespace Mgfx
