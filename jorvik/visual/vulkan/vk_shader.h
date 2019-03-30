#pragma once

#include "vulkan/vulkan.hpp"

namespace Mgfx
{

vk::UniqueShaderModule createShaderModule(vk::UniqueDevice &device, vk::ShaderStageFlagBits shaderStage, const fs::path& shaderPath, std::string const& shaderText, std::string& result);
bool GLSLtoSPV(const vk::ShaderStageFlagBits shaderType, const fs::path& shaderPath, std::string const& glslShader, std::vector<unsigned int> &spvShader, std::string& result);

} // namespace Mgfx
