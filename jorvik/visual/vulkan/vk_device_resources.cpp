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

    Create();
    CreateSwapChain();

    m_pDeviceNotify->OnCreateDeviceObjects();
}

void VkDeviceResources::Wait()
{
    device->waitIdle();
}

void VkDeviceResources::Destroy()
{
    m_pDeviceNotify->OnInvalidateDeviceObjects();

    DestroySwapChain();

    descriptorPool.reset();
    device.reset();

    vkDestroySurfaceKHR(instance.get(), surface, nullptr);

    debugCallback.reset();

    instance.reset();
}

void VkDeviceResources::DestroySwapChain()
{
    device->waitIdle();
    m_spWindow.reset();
    perFrame.clear();
}

void VkDeviceResources::RecreateSwapChain()
{
    LOG(DEBUG) << "Vulkan: RecreateSwapChain";

    m_pDeviceNotify->OnBeginResize();

    DestroySwapChain();
    CreateSwapChain();
    //CreateGraphicsPipeline();

    m_pDeviceNotify->OnEndResize();
}

bool VkDeviceResources::Create()
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
    m_lastFrame = m_currentFrame;

    // Wait for the current frame fence
    // This is so that the CPU doesn't get ahead of the GPU; up to the depth of the number of frames
    device->waitForFences(perFrame[m_lastFrame].inFlightFence.get(), VK_TRUE, std::numeric_limits<uint64_t>::max());

    // Need the last frame to submit from the previous semaphore
    VkResult result = vkAcquireNextImageKHR(device.get(), m_spWindow->Swapchain(), std::numeric_limits<uint64_t>::max(), *perFrame[m_lastFrame].imageAvailableSemaphore, VK_NULL_HANDLE, &m_currentFrame);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return false;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        return false;
    }

    auto& frame = perFrame[m_currentFrame];

    // ? Need this flag
    device->resetCommandPool(*frame.commandPool, vk::CommandPoolResetFlagBits::eReleaseResources);

    if (frame.commandBuffers.empty())
    {
        frame.commandBuffers = device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*frame.commandPool, vk::CommandBufferLevel::ePrimary, 1));
    }

    vk::ClearValue clearValues[2];
    clearValues[0].color = vk::ClearColorValue(std::array<float, 4>({ 100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f }));
    clearValues[1].depthStencil = vk::ClearDepthStencilValue(1.0f, 0);

    vk::RenderPassBeginInfo beginInfo(
        m_spWindow->RenderPass(),
        m_spWindow->Framebuffers()[m_currentFrame].get(),
        vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(m_spWindow->Width(), m_spWindow->Height())),
        2,
        clearValues);

    frame.commandBuffers[0]->begin(vk::CommandBufferBeginInfo());
    frame.commandBuffers[0]->beginRenderPass(beginInfo, vk::SubpassContents::eInline);

    return true;
}

bool VkDeviceResources::Present()
{
    auto& frame = perFrame[m_currentFrame];

    frame.commandBuffers[0]->endRenderPass();
    frame.commandBuffers[0]->end();

    device->resetFences(perFrame[m_currentFrame].inFlightFence.get());

    vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    vk::SubmitInfo submit(1, &*perFrame[m_lastFrame].imageAvailableSemaphore, &waitDestinationStageMask, 1, &*frame.commandBuffers[0], 1, &*frame.renderFinishedSemaphore);
    graphicsQueue().submit(1, &submit, *frame.inFlightFence);

    try
    {
        vk::PresentInfoKHR presentInfo(1, &*frame.renderFinishedSemaphore, 1, &m_spWindow->Swapchain(), &m_currentFrame);
        graphicsQueue().presentKHR(presentInfo);
    }
    catch (std::exception&)
    {
        framebufferResized = false;
        RecreateSwapChain();
        return false;
    }
    return true;
}

} // namespace Mgfx
