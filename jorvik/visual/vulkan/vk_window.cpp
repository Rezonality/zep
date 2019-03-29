#include "utils/logger.h"
#include "vk_device_resources.h"
#include "vk_window.h"

namespace Mgfx
{

Window::Window(const vk::Instance& instance, const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t graphicsQueueFamilyIndex, vk::SurfaceKHR surface)
{
    surface_ = surface;
    instance_ = instance;
    device_ = device;
    presentQueueFamily_ = 0;
    auto& pd = physicalDevice;
    auto qprops = pd.getQueueFamilyProperties();
    bool found = false;
    for (uint32_t qi = 0; qi != qprops.size(); ++qi)
    {
        auto& qprop = qprops[qi];
        if (pd.getSurfaceSupportKHR(qi, surface_) && (qprop.queueFlags & vk::QueueFlagBits::eGraphics) == vk::QueueFlagBits::eGraphics)
        {
            presentQueueFamily_ = qi;
            found = true;
            break;
        }
    }

    if (!found)
    {
        std::cout << "No Vulkan present queues found\n";
        return;
    }

    auto fmts = pd.getSurfaceFormatsKHR(surface_);
    swapchainImageFormat_ = fmts[0].format;
    swapchainColorSpace_ = fmts[0].colorSpace;
    if (fmts.size() == 1 && swapchainImageFormat_ == vk::Format::eUndefined)
    {
        swapchainImageFormat_ = vk::Format::eB8G8R8A8Unorm;
        swapchainColorSpace_ = vk::ColorSpaceKHR::eSrgbNonlinear;
    }
    else
    {
        for (auto& fmt : fmts)
        {
            if (fmt.format == vk::Format::eB8G8R8A8Unorm)
            {
                swapchainImageFormat_ = fmt.format;
                swapchainColorSpace_ = fmt.colorSpace;
            }
        }
    }

    auto surfaceCaps = pd.getSurfaceCapabilitiesKHR(surface_);
    width_ = surfaceCaps.currentExtent.width;
    height_ = surfaceCaps.currentExtent.height;

    auto pms = pd.getSurfacePresentModesKHR(surface_);
    vk::PresentModeKHR presentMode = pms[0];
    if (std::find(pms.begin(), pms.end(), vk::PresentModeKHR::eFifo) != pms.end())
    {
        presentMode = vk::PresentModeKHR::eFifo;
    }
    else
    {
        std::cout << "No fifo mode available\n";
        return;
    }

    //std::cout << "using " << vk::to_string(presentMode) << "\n";

    vk::SwapchainCreateInfoKHR swapinfo{};
    std::array<uint32_t, 2> queueFamilyIndices = { graphicsQueueFamilyIndex, presentQueueFamily_ };
    bool sameQueues = queueFamilyIndices[0] == queueFamilyIndices[1];
    vk::SharingMode sharingMode = !sameQueues ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive;
    swapinfo.imageExtent = surfaceCaps.currentExtent;
    swapinfo.surface = surface_;
    swapinfo.minImageCount = surfaceCaps.minImageCount + 1;
    swapinfo.imageFormat = swapchainImageFormat_;
    swapinfo.imageColorSpace = swapchainColorSpace_;
    swapinfo.imageExtent = surfaceCaps.currentExtent;
    swapinfo.imageArrayLayers = 1;
    swapinfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    swapinfo.imageSharingMode = sharingMode;
    swapinfo.queueFamilyIndexCount = !sameQueues ? 2 : 0;
    swapinfo.pQueueFamilyIndices = queueFamilyIndices.data();
    swapinfo.preTransform = surfaceCaps.currentTransform;
    ;
    swapinfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    swapinfo.presentMode = presentMode;
    swapinfo.clipped = 1;
    swapinfo.oldSwapchain = vk::SwapchainKHR{};
    swapchain_ = device.createSwapchainKHRUnique(swapinfo);

    images_ = device.getSwapchainImagesKHR(*swapchain_);
    for (auto& img : images_)
    {
        vk::ImageViewCreateInfo ci{};
        ci.image = img;
        ci.viewType = vk::ImageViewType::e2D;
        ci.format = swapchainImageFormat_;
        ci.subresourceRange = { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 };
        imageViews_.emplace_back(device.createImageView(ci));
    }

    auto memprops = physicalDevice.getMemoryProperties();
    depthStencilImage_ = vku::DepthStencilImage(device, memprops, width_, height_);

    // Build the renderpass using two attachments, colour and depth/stencil.
    vku::RenderpassMaker rpm;

    // The only colour attachment.
    rpm.attachmentBegin(swapchainImageFormat_);
    rpm.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rpm.attachmentStoreOp(vk::AttachmentStoreOp::eStore);
    rpm.attachmentFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    // The depth/stencil attachment.
    rpm.attachmentBegin(depthStencilImage_.format());
    rpm.attachmentLoadOp(vk::AttachmentLoadOp::eClear);
    rpm.attachmentStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    rpm.attachmentFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    // A subpass to render using the above two attachments.
    rpm.subpassBegin(vk::PipelineBindPoint::eGraphics);
    rpm.subpassColorAttachment(vk::ImageLayout::eColorAttachmentOptimal, 0);
    rpm.subpassDepthStencilAttachment(vk::ImageLayout::eDepthStencilAttachmentOptimal, 1);

    // A dependency to reset the layout of both attachments.
    rpm.dependencyBegin(VK_SUBPASS_EXTERNAL, 0);
    rpm.dependencySrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rpm.dependencyDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    rpm.dependencyDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

    // Use the maker object to construct the vulkan object
    renderPass_ = rpm.createUnique(device);

    for (int i = 0; i != imageViews_.size(); ++i)
    {
        vk::ImageView attachments[2] = { imageViews_[i], depthStencilImage_.imageView() };
        vk::FramebufferCreateInfo fbci{ {}, *renderPass_, 2, attachments, width_, height_, 1 };
        framebuffers_.push_back(device.createFramebufferUnique(fbci));
    }

    vk::SemaphoreCreateInfo sci;
    imageAcquireSemaphore_ = device.createSemaphoreUnique(sci);
    commandCompleteSemaphore_ = device.createSemaphoreUnique(sci);

    ok_ = true;
}

void Window::DumpCaps(std::ostream& os, vk::PhysicalDevice pd) const
{
    os << "Surface formats\n";
    auto fmts = pd.getSurfaceFormatsKHR(surface_);
    for (auto& fmt : fmts)
    {
        auto fmtstr = vk::to_string(fmt.format);
        auto cstr = vk::to_string(fmt.colorSpace);
        os << "format=" << fmtstr << " colorSpace=" << cstr << "\n";
    }

    os << "Present Modes\n";
    auto presentModes = pd.getSurfacePresentModesKHR(surface_);
    for (auto pm : presentModes)
    {
        std::cout << vk::to_string(pm) << "\n";
    }
}

/// Return the queue family index used to present the surface to the display.
uint32_t Window::PresentQueueFamily() const
{
    return presentQueueFamily_;
}

/// Get the queue used to submit graphics jobs
const vk::Queue Window::PresentQueue() const
{
    return device_.getQueue(presentQueueFamily_, 0);
}

bool Window::Ok() const
{
    return ok_;
}

/// Return the renderpass used by this window.
vk::RenderPass Window::RenderPass() const
{
    return *renderPass_;
}

/// Return the frame buffers used by this window
const std::vector<vk::UniqueFramebuffer>& Window::Framebuffers() const
{
    return framebuffers_;
}

/// Destroy resources when shutting down.
Window::~Window()
{
    for (auto& iv : imageViews_)
    {
        device_.destroyImageView(iv);
    }
    for (auto& f : commandBufferFences_)
    {
        device_.destroyFence(f);
    }
    swapchain_ = vk::UniqueSwapchainKHR{};
}

/// Return the width of the display.
uint32_t Window::Width() const
{
    return width_;
}

/// Return the height of the display.
uint32_t Window::Height() const
{
    return height_;
}

/// Return the format of the back buffer.
vk::Format Window::SwapchainImageFormat() const
{
    return swapchainImageFormat_;
}

/// Return the colour space of the back buffer (Usually sRGB)
vk::ColorSpaceKHR Window::SwapchainColorSpace() const
{
    return swapchainColorSpace_;
}

/// Return the swapchain object
const vk::SwapchainKHR Window::Swapchain() const
{
    return *swapchain_;
}

/// Return the views of the swap chain images
const std::vector<vk::ImageView>& Window::ImageViews() const
{
    return imageViews_;
}

/// Return the swap chain images
const std::vector<vk::Image>& Window::Images() const
{
    return images_;
}

/// Return the semaphore signalled when an image is acquired.
vk::Semaphore Window::ImageAcquireSemaphore() const
{
    return *imageAcquireSemaphore_;
}

/// Return the semaphore signalled when the command buffers are finished.
vk::Semaphore Window::CommandCompleteSemaphore() const
{
    return *commandCompleteSemaphore_;
}

/// Return the number of swap chain images.
int Window::NumImageIndices() const
{
    return (int)images_.size();
}

} // Mgfx
