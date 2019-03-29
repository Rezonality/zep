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
    CreateDevice();

    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    //CreateGraphicsPipeline();
    CreateFramebuffers();
    CreateCommandPool();
    CreateSyncObjects();
    CreateDescriptorPool();

    m_pDeviceNotify->OnCreateDeviceObjects();
}

void VkDeviceResources::Wait()
{
    device->waitIdle();
}

void VkDeviceResources::CleanupSwapChain()
{
    for (auto& frame : perFrame)
    {
        vkDestroyFramebuffer(device.get(), frame.swapChainFramebuffer, nullptr);
        vkFreeCommandBuffers(device.get(), frame.commandPool, static_cast<uint32_t>(frame.commandBuffers.size()), frame.commandBuffers.data());
        vkDestroyImageView(device.get(), frame.swapChainImageView, nullptr);
    }

    if (graphicsPipeline)
    {
        vkDestroyPipeline(device.get(), graphicsPipeline, nullptr);
        graphicsPipeline = nullptr;
    }

    if (pipelineLayout)
    {
        vkDestroyPipelineLayout(device.get(), pipelineLayout, nullptr);
        pipelineLayout = nullptr;
    }

    if (renderPass)
    {
        vkDestroyRenderPass(device.get(), renderPass, nullptr);
        renderPass = nullptr;
    }

    vkDestroySwapchainKHR(device.get(), swapChain, nullptr);
}

void VkDeviceResources::Cleanup()
{
    m_pDeviceNotify->OnInvalidateDeviceObjects();

    CleanupSwapChain();

    for (auto& frame : perFrame)
    {
        vkDestroySemaphore(device.get(), frame.renderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device.get(), frame.imageAvailableSemaphore, nullptr);
        vkDestroyFence(device.get(), frame.inFlightFence, nullptr);
        vkDestroyCommandPool(device.get(), frame.commandPool, nullptr);
    }

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

    CleanupSwapChain();

    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass(); // Note: surface format could have changed
    //CreateGraphicsPipeline();k
    CreateFramebuffers();

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
    return true;
}

void VkDeviceResources::CreateDescriptorPool()
{
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
}

bool VkDeviceResources::CreateDevice()
{
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

    return true;
}

void VkDeviceResources::CreateSwapChain()
{
    SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport();

    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
    {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies();
    uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

    if (indices.graphicsFamily != indices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device.get(), &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(device.get(), swapChain, &imageCount, nullptr);

    perFrame.resize(imageCount);

    std::vector<VkImage> images;
    images.resize(imageCount);

    vkGetSwapchainImagesKHR(device.get(), swapChain, &imageCount, images.data());
    for (uint32_t i = 0; i < imageCount; i++)
    {
        perFrame[i].swapChainImage = images[i];
    }

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}

void VkDeviceResources::CreateImageViews()
{
    for (size_t i = 0; i < perFrame.size(); i++)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = perFrame[i].swapChainImage;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device.get(), &createInfo, nullptr, &perFrame[i].swapChainImageView) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void VkDeviceResources::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device.get(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VkDeviceResources::CreateGraphicsPipeline()
{
    auto vertShaderCode = file_read("shaders/vert.spv");
    auto fragShaderCode = file_read("shaders/frag.spv");

    VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapChainExtent.width;
    viewport.height = (float)swapChainExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = swapChainExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(device.get(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device.get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device.get(), fragShaderModule, nullptr);
    vkDestroyShaderModule(device.get(), vertShaderModule, nullptr);
}

void VkDeviceResources::CreateFramebuffers()
{
    for (size_t i = 0; i < perFrame.size(); i++)
    {
        VkImageView attachments[] = {
            perFrame[i].swapChainImageView
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device.get(), &framebufferInfo, nullptr, &perFrame[i].swapChainFramebuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VkDeviceResources::CreateCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = FindQueueFamilies();

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    for (auto& frame : perFrame)
    {
        if (vkCreateCommandPool(device.get(), &poolInfo, nullptr, &frame.commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }
}

/*
void VkDeviceResources::CreateCommandBuffers()
{
    commandBuffers.resize(swapChainFramebuffers.size());


    /*
    for (size_t i = 0; i < commandBuffers.size(); i++)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;

        VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}
*/

void VkDeviceResources::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < perFrame.size(); i++)
    {
        if (
            vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &perFrame[i].imageAvailableSemaphore) != VK_SUCCESS || vkCreateSemaphore(device.get(), &semaphoreInfo, nullptr, &perFrame[i].renderFinishedSemaphore) != VK_SUCCESS || vkCreateFence(device.get(), &fenceInfo, nullptr, &perFrame[i].inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void VkDeviceResources::Prepare()
{
    // Wait for the current frame fence
    // This is so that the CPU doesn't get ahead of the GPU; up to the depth of the number of frames
    vkWaitForFences(device.get(), 1, &perFrame[currentFrame].inFlightFence, VK_TRUE, std::numeric_limits<uint64_t>::max());

    // Need the last frame to submit from the previous semaphore
    lastFrame = currentFrame;
    VkResult result = vkAcquireNextImageKHR(device.get(), swapChain, std::numeric_limits<uint64_t>::max(), perFrame[currentFrame].imageAvailableSemaphore, VK_NULL_HANDLE, &currentFrame);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    // ? Need this flag
    vkResetCommandPool(device.get(), perFrame[currentFrame].commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);

    perFrame[currentFrame].commandBuffers.resize(1);
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = perFrame[currentFrame].commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device.get(), &allocInfo, perFrame[currentFrame].commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // Temp: cornflower blue
    std::vector<float> clear_color = { 100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.00f };
    VkClearValue clear;
    memcpy(&clear.color.float32[0], &clear_color[0], sizeof(float) * 4);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    auto err = vkBeginCommandBuffer(perFrame[currentFrame].commandBuffers[0], &begin_info);
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = renderPass;
        info.framebuffer = perFrame[currentFrame].swapChainFramebuffer;
        info.renderArea.extent.width = swapChainExtent.width;
        info.renderArea.extent.height = swapChainExtent.height;
        info.clearValueCount = 1;
        info.pClearValues = &clear;
        vkCmdBeginRenderPass(perFrame[currentFrame].commandBuffers[0], &info, VK_SUBPASS_CONTENTS_INLINE);
    }
}

void VkDeviceResources::Present()
{
    vkCmdEndRenderPass(perFrame[currentFrame].commandBuffers[0]);
    vkEndCommandBuffer(perFrame[currentFrame].commandBuffers[0]);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { perFrame[lastFrame].imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &perFrame[currentFrame].commandBuffers[0];

    VkSemaphore signalSemaphores[] = { perFrame[currentFrame].renderFinishedSemaphore };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(device.get(), 1, &perFrame[currentFrame].inFlightFence);
    if (vkQueueSubmit(graphicsQueue(), 1, &submitInfo, perFrame[currentFrame].inFlightFence) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &currentFrame;

    VkResult result = vkQueuePresentKHR(graphicsQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
        framebufferResized = false;
        RecreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("failed to present swap chain image!");
    }
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

VkSurfaceFormatKHR VkDeviceResources::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        return { VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
    }

    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR VkDeviceResources::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return availablePresentMode;
        }
        else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            bestMode = availablePresentMode;
        }
    }

    return bestMode;
}

VkExtent2D VkDeviceResources::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        int width, height;
        SDL_GetWindowSize(m_pWindow, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

SwapChainSupportDetails VkDeviceResources::QuerySwapChainSupport()
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

QueueFamilyIndices VkDeviceResources::FindQueueFamilies()
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if (queueFamily.queueCount > 0 && presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

} // namespace Mgfx
