#include "utils/logger.h"
#include "vk_device_resources.h"

#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/StandAlone/DirStackFileIncluder.h"

namespace Mgfx
{

EShLanguage translateShaderStage(vk::ShaderStageFlagBits stage)
{
    switch (stage)
    {
    case vk::ShaderStageFlagBits::eVertex:
        return EShLangVertex;
    case vk::ShaderStageFlagBits::eTessellationControl:
        return EShLangTessControl;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
        return EShLangTessEvaluation;
    case vk::ShaderStageFlagBits::eGeometry:
        return EShLangGeometry;
    case vk::ShaderStageFlagBits::eFragment:
        return EShLangFragment;
    case vk::ShaderStageFlagBits::eCompute:
        return EShLangCompute;
    default:
        assert(false && "Unknown shader stage");
        return EShLangVertex;
    }
}

void init(TBuiltInResource& resource)
{
    resource.maxLights = 32;
    resource.maxClipPlanes = 6;
    resource.maxTextureUnits = 32;
    resource.maxTextureCoords = 32;
    resource.maxVertexAttribs = 64;
    resource.maxVertexUniformComponents = 4096;
    resource.maxVaryingFloats = 64;
    resource.maxVertexTextureImageUnits = 32;
    resource.maxCombinedTextureImageUnits = 80;
    resource.maxTextureImageUnits = 32;
    resource.maxFragmentUniformComponents = 4096;
    resource.maxDrawBuffers = 32;
    resource.maxVertexUniformVectors = 128;
    resource.maxVaryingVectors = 8;
    resource.maxFragmentUniformVectors = 16;
    resource.maxVertexOutputVectors = 16;
    resource.maxFragmentInputVectors = 15;
    resource.minProgramTexelOffset = -8;
    resource.maxProgramTexelOffset = 7;
    resource.maxClipDistances = 8;
    resource.maxComputeWorkGroupCountX = 65535;
    resource.maxComputeWorkGroupCountY = 65535;
    resource.maxComputeWorkGroupCountZ = 65535;
    resource.maxComputeWorkGroupSizeX = 1024;
    resource.maxComputeWorkGroupSizeY = 1024;
    resource.maxComputeWorkGroupSizeZ = 64;
    resource.maxComputeUniformComponents = 1024;
    resource.maxComputeTextureImageUnits = 16;
    resource.maxComputeImageUniforms = 8;
    resource.maxComputeAtomicCounters = 8;
    resource.maxComputeAtomicCounterBuffers = 1;
    resource.maxVaryingComponents = 60;
    resource.maxVertexOutputComponents = 64;
    resource.maxGeometryInputComponents = 64;
    resource.maxGeometryOutputComponents = 128;
    resource.maxFragmentInputComponents = 128;
    resource.maxImageUnits = 8;
    resource.maxCombinedImageUnitsAndFragmentOutputs = 8;
    resource.maxCombinedShaderOutputResources = 8;
    resource.maxImageSamples = 0;
    resource.maxVertexImageUniforms = 0;
    resource.maxTessControlImageUniforms = 0;
    resource.maxTessEvaluationImageUniforms = 0;
    resource.maxGeometryImageUniforms = 0;
    resource.maxFragmentImageUniforms = 8;
    resource.maxCombinedImageUniforms = 8;
    resource.maxGeometryTextureImageUnits = 16;
    resource.maxGeometryOutputVertices = 256;
    resource.maxGeometryTotalOutputComponents = 1024;
    resource.maxGeometryUniformComponents = 1024;
    resource.maxGeometryVaryingComponents = 64;
    resource.maxTessControlInputComponents = 128;
    resource.maxTessControlOutputComponents = 128;
    resource.maxTessControlTextureImageUnits = 16;
    resource.maxTessControlUniformComponents = 1024;
    resource.maxTessControlTotalOutputComponents = 4096;
    resource.maxTessEvaluationInputComponents = 128;
    resource.maxTessEvaluationOutputComponents = 128;
    resource.maxTessEvaluationTextureImageUnits = 16;
    resource.maxTessEvaluationUniformComponents = 1024;
    resource.maxTessPatchComponents = 120;
    resource.maxPatchVertices = 32;
    resource.maxTessGenLevel = 64;
    resource.maxViewports = 16;
    resource.maxVertexAtomicCounters = 0;
    resource.maxTessControlAtomicCounters = 0;
    resource.maxTessEvaluationAtomicCounters = 0;
    resource.maxGeometryAtomicCounters = 0;
    resource.maxFragmentAtomicCounters = 8;
    resource.maxCombinedAtomicCounters = 8;
    resource.maxAtomicCounterBindings = 1;
    resource.maxVertexAtomicCounterBuffers = 0;
    resource.maxTessControlAtomicCounterBuffers = 0;
    resource.maxTessEvaluationAtomicCounterBuffers = 0;
    resource.maxGeometryAtomicCounterBuffers = 0;
    resource.maxFragmentAtomicCounterBuffers = 1;
    resource.maxCombinedAtomicCounterBuffers = 1;
    resource.maxAtomicCounterBufferSize = 16384;
    resource.maxTransformFeedbackBuffers = 4;
    resource.maxTransformFeedbackInterleavedComponents = 64;
    resource.maxCullDistances = 8;
    resource.maxCombinedClipAndCullDistances = 8;
    resource.maxSamples = 4;
    resource.limits.nonInductiveForLoops = 1;
    resource.limits.whileLoops = 1;
    resource.limits.doWhileLoops = 1;
    resource.limits.generalUniformIndexing = 1;
    resource.limits.generalAttributeMatrixVectorIndexing = 1;
    resource.limits.generalVaryingIndexing = 1;
    resource.limits.generalSamplerIndexing = 1;
    resource.limits.generalVariableIndexing = 1;
    resource.limits.generalConstantMatrixVectorIndexing = 1;
}

bool GLSLtoSPV(const vk::ShaderStageFlagBits shaderType, const fs::path& shaderPath, std::string const& shaderText, std::vector<unsigned int>& spvShader)
{
    EShLanguage stage = translateShaderStage(shaderType);

    TBuiltInResource resource;
    init(resource);

    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
    const int defaultVersion = 100;

    DirStackFileIncluder Includer;
    if (shaderPath.has_parent_path())
    {
        Includer.pushExternalLocalDirectory(shaderPath.parent_path().string());
    }

    std::string preprocessed;
    glslang::TShader shader(stage);

    //Set up Vulkan/SpirV Environment
	int clientInputSemanticsVersion = 100;                                               // maps to, say, #define VULKAN 100
	glslang::EShTargetClientVersion VulkanClientVersion = glslang::EShTargetVulkan_1_0;  // would map to, say, Vulkan 1.0
	glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0;         // maps to, say, SPIR-V 1.0

    shader.setEnvInput(glslang::EShSourceHlsl, stage, glslang::EShClientVulkan, clientInputSemanticsVersion);
    shader.setEnvClient(glslang::EShClientVulkan, VulkanClientVersion);
    shader.setEnvTarget(glslang::EShTargetSpv, TargetVersion);

    const char* pInput = shaderText.c_str();
    shader.setStrings(&pInput, 1);

    if (!shader.preprocess(&resource, defaultVersion, ENoProfile, false, false, messages, &preprocessed, Includer))
    {
        LOG(INFO) << "PreProcess fail: " << shaderPath;
        LOG(INFO) << shader.getInfoLog();
        LOG(INFO) << shader.getInfoDebugLog();
        return false;
    }

    const char* pProcessed = preprocessed.c_str();
    shader.setStrings(&pProcessed, 1);

    if (!shader.parse(&resource, 100, false, messages))
    {
        LOG(DEBUG) << shader.getInfoLog();
        LOG(DEBUG) << shader.getInfoDebugLog();
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages))
    {
        LOG(INFO) << shader.getInfoLog();
        LOG(INFO) << shader.getInfoDebugLog();
        return false;
    }

    glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);
    return true;
}

vk::UniqueShaderModule createShaderModule(vk::UniqueDevice& device, vk::ShaderStageFlagBits shaderStage, const fs::path& shaderPath, std::string const& shaderText)
{
    std::vector<unsigned int> shaderSPV;
    bool ok = GLSLtoSPV(shaderStage, shaderPath, shaderText, shaderSPV);
    if (!ok)
        return vk::UniqueShaderModule();

    return device->createShaderModuleUnique(vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), shaderSPV.size() * sizeof(unsigned int), shaderSPV.data()));
}

} // namespace Mgfx
