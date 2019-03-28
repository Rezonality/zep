#pragma once
#include "visual/IDevice.h"
#include "vookoo/include/vku/vku.hpp"
#include <optional>
#include <vector>

struct SDL_Window;

namespace Mgfx
{

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
    void CleanupSwapChain();
    void RecreateSwapChain();
    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void CreateDevice();
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

    VkShaderModule CreateShaderModule(const std::string& code);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapChainSupportDetails QuerySwapChainSupport();
    bool isDeviceSuitable();
    bool CheckDeviceExtensionSupport();
    QueueFamilyIndices FindQueueFamilies();

    SDL_Window* m_pWindow;

    vk::UniqueInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger = nullptr;
    VkSurfaceKHR surface = nullptr;

    vk::UniqueDevice device;
    vk::PhysicalDevice physicalDevice;
    vk::PhysicalDeviceMemoryProperties memprops;

    VkQueue presentQueue = nullptr;

    std::vector<FrameData> perFrame;
    uint32_t currentFrame = 0;
    uint32_t lastFrame = 0;

    VkSwapchainKHR swapChain = nullptr;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    VkRenderPass renderPass = nullptr;
    VkPipelineLayout pipelineLayout = nullptr;
    VkPipeline graphicsPipeline = nullptr;
    VkDescriptorPool descriptorPool = nullptr;

    uint32_t graphicsQueueFamilyIndex;
    uint32_t computeQueueFamilyIndex;

    bool framebufferResized = false;
    Mgfx::IDeviceNotify* m_pDeviceNotify;
};

} // namespace Mgfx
