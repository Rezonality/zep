#pragma once

#include "visual/IDevice.h"
#include "vookoo/include/vku/vku.hpp"
#include <optional>
#include <vector>
#include "vk_debug_callback.h"

struct SDL_Window;

namespace Mgfx
{

class VkWindow;
class VkDebugCallback;
struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

struct FrameData
{
    vk::UniqueCommandPool commandPool;
    std::vector<vk::UniqueCommandBuffer> commandBuffers;
    vk::UniqueSemaphore imageAvailableSemaphore;
    vk::UniqueSemaphore renderFinishedSemaphore;
    vk::UniqueFence inFlightFence;
};

class VkDeviceResources
{
public:
    VkDeviceResources(IDeviceNotify* pNotify);
    ~VkDeviceResources();

    void Init(SDL_Window* pWindow);
    void Wait();
    void Cleanup();
    bool CreateInstance();
    bool CreateSwapChain();

    void RecreateSwapChain();

    bool Prepare();
    bool Present();

    FrameData& GetCurrentFrame()
    {
        return perFrame[currentFrame];
    }

    /// Get the queue used to submit graphics jobs
    const vk::Queue graphicsQueue() const
    {
        return device->getQueue(graphicsQueueFamilyIndex, 0);
    }

    /// Get the queue used to submit compute jobs
    const vk::Queue computeQueue() const
    {
        return device->getQueue(computeQueueFamilyIndex, 0);
    }

    const VkWindow* GetWindow() const
    {
        return m_spWindow.get();
    }

    SDL_Window* m_pWindow;

    vk::UniqueInstance instance;
    vk::SurfaceKHR surface;
    vk::UniqueDevice device;
    vk::PhysicalDevice physicalDevice;
    vk::PhysicalDeviceMemoryProperties memprops;
    vk::UniqueDescriptorPool descriptorPool;
    
    uint32_t graphicsQueueFamilyIndex;
    uint32_t computeQueueFamilyIndex;

    VkDebugCallback debugCallback;

    std::unique_ptr<VkWindow> m_spWindow;

    VkShaderModule CreateShaderModule(const std::string& code);

    std::vector<FrameData> perFrame;

    uint32_t currentFrame = 0;
    uint32_t lastFrame = 0;

    //VkRenderPass renderPass = nullptr;
    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline graphicsPipeline = nullptr;

    bool framebufferResized = false;
    Mgfx::IDeviceNotify* m_pDeviceNotify;
};

} // namespace Mgfx
