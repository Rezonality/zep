#pragma once

#include "vulkan/vulkan.hpp"

namespace Mgfx
{

std::shared_ptr<CompiledShaderAssetVulkan> GLSLangCompile(vk::UniqueDevice& device, vk::ShaderStageFlagBits shaderStage, const fs::path& shaderPath, const std::string& shaderText);

} // namespace Mgfx
