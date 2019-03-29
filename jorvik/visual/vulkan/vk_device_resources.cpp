#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <stdexcept>

#include "utils/file/file.h"
#include "utils/logger.h"

#include "SDL.h"
#include "SDL_vulkan.h"
#include "vk_device_resources.h"
#include "vk_window.h"

namespace Mgfx
{

VkDeviceResources::VkDeviceResources(Mgfx::IDeviceNotify* pNotify)
    : m_pDeviceNotify(pNotify)
{
}

VkDeviceResources::~VkDeviceResources()
{
}

void VkDeviceResources::Init(SDL_Window* pWindow)
{
    m_pWindow = pWindow;
    CreateInstance();
    CreateSwapChain();

    m_pDeviceNotify->OnCreateDeviceObjects();
}

void VkDeviceResources::Wait()
{
    device->waitIdle();
}

void VkDeviceResources::Cleanup()
{
    m_pDeviceNotify->OnInvalidateDeviceObjects();

    m_spWindow.reset();
    perFrame.clear();
    descriptorPool.reset();
    device.reset();

    vkDestroySurfaceKHR(instance.get(), surface, nullptr);

    debugCallback.reset();

    instance.reset();
}

void VkDeviceResources::RecreateSwapChain()
{
    LOG(DEBUG) << "Vulkan: RecreateSwapChain";

    m_pDeviceNotify->OnBeginResize();

    int width = 0, height = 0;
    SDL_GetWindowSize(m_pWindow, &width, &height);
    device->waitIdle();

    m_spWindow.reset();
    perFrame.clear();

    CreateSwapChain();
    //CreateGraphicsPipeline();k

    m_pDeviceNotify->OnEndResize();
}

bool VkDeviceResources::CreateInstance()
{
    // Get SDL required extensions
    uint32_t extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(m_pWindow, &extensionCount, nullptr);
    std::vector<const char*> sdlExtensions(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(m_pWindow, &extensionCount, sdlExtensions.data());

    // Make the vulkan instance
    vku::InstanceMaker im;
    im.defaultLayers();
    for (auto sdlExt : sdlExtensions)
    {
        im.extension(sdlExt);
    }
    im.extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    instance = im.createUnique();

    // Add debug callbacks
    debugCallback = VkDebugCallback(*instance);

    // Create the surface
    VkSurfaceKHR surf;
    if (!SDL_Vulkan_CreateSurface(m_pWindow, instance.get(), &surf))
    {
        return false;
    }

    surface = surf;

    auto pds = instance->enumeratePhysicalDevices();
    physicalDevice = pds[0];

    auto qprops = physicalDevice.getQueueFamilyProperties();
    const auto badQueue = ~(uint32_t)0;
    graphicsQueueFamilyIndex = badQueue;
    computeQueueFamilyIndex = badQueue;
    vk::QueueFlags search = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    // Look for an omnipurpose queue family first
    // It is better if we can schedule operations without barriers and semaphores.
    // The Spec says: "If an implementation exposes any queue family that supports graphics operations,
    // at least one queue family of at least one physical device exposed by the implementation
    // must support both graphics and compute operations."
    // Also: All commands that are allowed on a queue that supports transfer operations are
    // also allowed on a queue that supports either graphics or compute operations...
    // As a result we can expect a queue family with at least all three and maybe all four modes.
    for (uint32_t qi = 0; qi != qprops.size(); ++qi)
    {
        auto& qprop = qprops[qi];
        LOG(INFO) << vk::to_string(qprop.queueFlags);
        if ((qprop.queueFlags & search) == search)
        {
            graphicsQueueFamilyIndex = qi;
            computeQueueFamilyIndex = qi;
            break;
        }
    }

    if (graphicsQueueFamilyIndex == badQueue || computeQueueFamilyIndex == badQueue)
    {
        LOG(ERROR) << "Missing a queue";
        return false;
    }

    memprops = physicalDevice.getMemoryProperties();

    // todo: find optimal texture format
    // auto rgbaprops = physical_device_.getFormatProperties(vk::Format::eR8G8B8A8Unorm);
    vku::DeviceMaker dm{};
    dm.defaultLayers();
    dm.queue(graphicsQueueFamilyIndex);

    if (computeQueueFamilyIndex != graphicsQueueFamilyIndex)
    {
        dm.queue(computeQueueFamilyIndex);
    }
    device = dm.createUnique(physicalDevice);

    std::vector<vk::DescriptorPoolSize> poolSizes;
    poolSizes.emplace_back(vk::DescriptorType::eUniformBuffer, 128);
    poolSizes.emplace_back(vk::DescriptorType::eCombinedImageSampler, 128);
    poolSizes.emplace_back(vk::DescriptorType::eStorageBuffer, 128);

    // Create an arbitrary number of descriptors in a pool.
    // Allow the descriptors to be freed, possibly not optimal behaviour.
    vk::DescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    descriptorPoolInfo.maxSets = 256;
    descriptorPoolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    descriptorPoolInfo.pPoolSizes = poolSizes.data();
    descriptorPool = device->createDescriptorPoolUnique(descriptorPoolInfo);

    return true;
}

bool VkDeviceResources::CreateSwapChain()
{
    m_spWindow = std::make_unique<VkWindow>(instance.get(), device.get(), physicalDevice, graphicsQueueFamilyIndex, surface);
    if (!m_spWindow->Ok())
    {
        m_spWindow.reset();
        return false;
    }

    perFrame.resize(m_spWindow->NumImageIndices());

    for (auto& frame : perFrame)
    {
        frame.imageAvailableSemaphore = device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
        frame.renderFinishedSemaphore = device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
        frame.inFlightFence = device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
        frame.commandPool = device->createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsQueueFamilyIndex));
    }
    return true;
}

bool VkDeviceResources::Prepare()
{
    // Wait for the current frame fence
    // This is so that the CPU doesn't get ahead of the GPU; up to the depth of the number of frames
    device->waitForFences(perFrame[currentFrame].inFlightFence.get(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    // Need the last frame to submit from the previous semaphore
    lastFrame = currentFrame;
    VkResult result = vkAcquireNextImageKHR(device.get(), m_spWindow->Swapchain(), std::numeric_limits<uint64_t>::max(), *perFrame[currentFrame].imageAvailableSemaphore, VK_NULL_HANDLE, &currentFrame);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        return false;
    }

    // ? Need this flag
    device->resetCommandPool(*perFrame[currentFrame].commandPool, vk::CommandPoolResetFlagBits::eReleaseResources);

    perFrame[currentFrame].commandBuffers.resize(1);
    perFrame[currentFrame].commandBuffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*perFrame[currentFrame].commandPool, vk::CommandBufferLevel::ePrimary, 1));

    vk::ClearValue clearValues[2];
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({ 100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f }));
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderPassBeginInfo beginInfo(
        m_spWindow->RenderPass(),
        m_spWindow->Framebuffers()[currentFrame].get(),
        vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_spWindow->Width(), m_spWindow->Height())),
        2,
        clearValues);

    perFrame[currentFrame].commandBuffers[0]->begin(vk::CommandBufferBeginInfo());
    perFrame[currentFrame].commandBuffers[0]->beginRenderPass(beginInfo, vk::SubpassContents::eInline);

    return true;
}

bool VkDeviceResources::Present()
{
    perFrame[currentFrame].commandBuffers[0]->endRenderPass();
    perFrame[currentFrame].commandBuffers[0]->end();

    device->resetFences(perFrame[currentFrame].inFlightFence.get());

    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submit(1, &*perFrame[lastFrame].imageAvailableSemaphore, &waitDestinationStageMask, 1, &*perFrame[currentFrame].commandBuffers[0], 1, &*perFrame[currentFrame].renderFinishedSemaphore);
    graphicsQueue().submit(1, &submit, *perFrame[currentFrame].inFlightFence);

    try
    {
        vk::PresentInfoKHR presentInfo(1, &*perFrame[currentFrame].renderFinishedSemaphore, 1, &m_spWindow->Swapchain(), &currentFrame);
        graphicsQueue().presentKHR(presentInfo);
    }
    catch(std::exception&)
    {
        framebufferResized = false;
        RecreateSwapChain();
        return false;
    }
    return true;
}

VkShaderModule VkDeviceResources::CreateShaderModule(const std::string& code)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.c_str());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device.get(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

} // namespace Mgfx
