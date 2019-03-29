#pragma once

namespace Mgfx
{

class VkDebugCallback
{
public:
    VkDebugCallback();
    VkDebugCallback(
        vk::Instance instance,
        vk::DebugReportFlagsEXT flags = vk::DebugReportFlagBitsEXT::eWarning | /*vk::DebugReportFlagBitsEXT::eInformation |*/ vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::ePerformanceWarning);
    ~VkDebugCallback();
    void reset();

private:
    vk::DebugReportCallbackEXT m_callback;
    vk::Instance m_instance;
};
} // namespace Mgfx
