#pragma once

//#define VULKAN_HPP_NO_EXCEPTIONS 1

#include "visual/IDevice.h"
#include "vookoo/include/vku/vku.hpp"
#include <optional>
#include <vector>
#include "vk_debug_callback.h"

struct SDL_Window;

namespace Mgfx
{

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
    VkImage swapChainImage;
    VkImageView swapChainImageView;
    VkFramebuffer swapChainFramebuffer;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
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
    bool CreateDevice();

    void CleanupSwapChain();
    void RecreateSwapChain();
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateSyncObjects();
    void CreateCommandPool();
    void CreateGraphicsPipeline();
    void CreateDescriptorPool();

    void Prepare();
    void Present();

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

    VkShaderModule CreateShaderModule(const std::string& code);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapChainSupportDetails QuerySwapChainSupport();
    QueueFamilyIndices FindQueueFamilies();

    std::vector<FrameData> perFrame;

    uint32_t currentFrame = 0;
    uint32_t lastFrame = 0;

    VkSwapchainKHR swapChain = nullptr;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    VkRenderPass renderPass = nullptr;
    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline graphicsPipeline = nullptr;

    bool framebufferResized = false;
    Mgfx::IDeviceNotify* m_pDeviceNotify;
};

} // namespace Mgfx
