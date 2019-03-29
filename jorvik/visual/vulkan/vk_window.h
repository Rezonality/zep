#pragma once

#include <vulkan/vulkan.hpp>
#include <vookoo/include/vku/vku.hpp>

namespace Mgfx
{

class Window
{
public:
    Window(const vk::Instance& instance, const vk::Device& device, const vk::PhysicalDevice& physicalDevice, uint32_t graphicsQueueFamilyIndex, vk::SurfaceKHR surface);
    ~Window();

    Window& operator=(Window&& rhs) = default;

    void DumpCaps(std::ostream& os, vk::PhysicalDevice pd) const;

    uint32_t PresentQueueFamily() const;
    const vk::Queue PresentQueue() const;
    bool Ok() const;
    vk::RenderPass RenderPass() const;
    const std::vector<vk::UniqueFramebuffer>& Framebuffers() const;

    uint32_t Width() const;
    uint32_t Height() const;
    vk::Format SwapchainImageFormat() const;
    vk::ColorSpaceKHR SwapchainColorSpace() const;
    const vk::SwapchainKHR Swapchain() const;
    const std::vector<vk::ImageView>& ImageViews() const;
    const std::vector<vk::Image>& Images() const;

    vk::Semaphore ImageAcquireSemaphore() const;
    vk::Semaphore CommandCompleteSemaphore() const;
    int NumImageIndices() const;

private:
    vk::Instance instance_;
    vk::SurfaceKHR surface_;
    vk::UniqueSwapchainKHR swapchain_;
    vk::UniqueRenderPass renderPass_;
    vk::UniqueSemaphore imageAcquireSemaphore_;
    vk::UniqueSemaphore commandCompleteSemaphore_;

    std::vector<vk::ImageView> imageViews_;
    std::vector<vk::Image> images_;
    std::vector<vk::Fence> commandBufferFences_;
    std::vector<vk::UniqueFramebuffer> framebuffers_;
    vku::DepthStencilImage depthStencilImage_;

    uint32_t presentQueueFamily_ = 0;
    uint32_t width_;
    uint32_t height_;
    vk::Format swapchainImageFormat_ = vk::Format::eB8G8R8A8Unorm;
    vk::ColorSpaceKHR swapchainColorSpace_ = vk::ColorSpaceKHR::eSrgbNonlinear;
    vk::Device device_;
    bool ok_ = false;
};

} // Mgfx
