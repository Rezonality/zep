#pragma once

#include "vulkan/vulkan.hpp"

namespace Mgfx
{

vk::UniqueShaderModule createShaderModule(vk::UniqueDevice &device, vk::ShaderStageFlagBits shaderStage, std::string const& shaderText);
bool GLSLtoSPV(const vk::ShaderStageFlagBits shaderType, std::string const& glslShader, std::vector<unsigned int> &spvShader);

} // namespace Mgfx
