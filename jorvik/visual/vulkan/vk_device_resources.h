#pragma once
#include "vulkan/vulkan.h"
#include <optional>
#include <vector>
#include "visual/IDevice.h"

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
    void PickPhysicalDevice();
    void CreateLogicalDevice();
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

    VkShaderModule CreateShaderModule(const std::string& code);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    bool isDeviceSuitable(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    std::vector<const char*> GetRequiredExtensions();
    bool CheckValidationLayerSupport();

    SDL_Window* m_pWindow;

    VkInstance instance = nullptr;
    VkDebugUtilsMessengerEXT debugMessenger = nullptr;
    VkSurfaceKHR surface = nullptr;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = nullptr;

    VkQueue graphicsQueue = nullptr;
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

    QueueFamilyIndices queueFamilyIndices;

    bool framebufferResized = false;
    Mgfx::IDeviceNotify* m_pDeviceNotify;
};

} // Mgfx

