#include "vk_device_resources.h"
#include "utils/logger.h"

namespace Mgfx
{

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    LOG(DEBUG) << "Vulcan Validation Layer: " << pCallbackData->pMessage;
    return VK_FALSE;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData)
{
    LOG(DEBUG) << "VkDebug: " << pMessage << " (layer: " << pLayerPrefix << ")"; 
    return VK_FALSE;
}

VkDebugCallback::VkDebugCallback()
{
}

VkDebugCallback::VkDebugCallback( vk::Instance instance, vk::DebugReportFlagsEXT flags)
    : m_instance(instance)
{
    auto ci = vk::DebugReportCallbackCreateInfoEXT{ flags, &debugCallback };

    auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)instance.getProcAddr("vkCreateDebugReportCallbackEXT");

    VkDebugReportCallbackEXT cb;
    vkCreateDebugReportCallbackEXT(instance, &(const VkDebugReportCallbackCreateInfoEXT&)ci, nullptr, &cb);
    m_callback = cb;
}

VkDebugCallback::~VkDebugCallback()
{
}

void VkDebugCallback::reset()
{
    if (m_callback)
    {
        auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)m_instance.getProcAddr( "vkDestroyDebugReportCallbackEXT");
        vkDestroyDebugReportCallbackEXT(m_instance, m_callback, nullptr);
        m_callback = vk::DebugReportCallbackEXT{};
    }
}

} // namespace Mgfx
