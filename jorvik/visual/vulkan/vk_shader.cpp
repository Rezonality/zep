#include "utils/logger.h"
#include "utils/string/stringutils.h"
#include "vk_device_resources.h"

#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/StandAlone/DirStackFileIncluder.h"

#include "device_vulkan.h"

namespace Mgfx
{

std::map<std::string, EShLanguage> FileNameToLang = 
{
    { "ps", EShLangFragment },
    { "vs", EShLangVertex },
    { "gs", EShLangGeometry },
    { "cs", EShLangCompute }
};

// If not found in the meta tags, then assign based on file and just use '5'
EShLanguage GetShaderType(std::shared_ptr<CompiledShaderAssetVulkan>& spResult, const fs::path& path)
{
    std::string shaderType = spResult->spTags->shader_type.value;
    std::string search = shaderType.empty() ? path.stem().string() : shaderType;
    for (auto& current : FileNameToLang)
    {
        if (search == current.first)
        {
            return current.second;
        }
    }
    return EShLangFragment;
}

// If not found in the meta tags, then hope for 'main'
std::string GetEntryPoint(std::shared_ptr<CompiledShaderAssetVulkan>& spResult)
{
    std::string entryPoint = spResult->spTags->entry_point.value;
    if (entryPoint.empty())
    {
        return "main";
    }
    return entryPoint;
}

// We need to filter:
// ERROR : (<linenum>,<column>-?<column>): message
// This can probably done efficiently with regex, but I'm no expert on regex,
// and it's easy to miss thing. So here we just do simple string searches
// It works, and makes up an error when it doesn't, so I can fix it!
void ParseErrors(std::shared_ptr<CompiledShaderAssetVulkan>& spResult, const std::string& output)
{
    LOG(DEBUG) << output;

    // Just in case we get more than one line
    std::vector<std::string> errors;
    string_split(output, "\n", errors);
    for (auto error : errors)
    {
        auto pMsg = std::make_shared<CompileMessage>();
        pMsg->filePath = spResult->path.string();

        try
        {
            auto messageBreakPos = error.find_first_of(':');
            if (messageBreakPos != std::string::npos)
            {
                auto type = string_trim(error.substr(0, messageBreakPos));
                auto message = string_trim(error.substr(messageBreakPos + 1, error.size() - messageBreakPos + 1));

                pMsg->text = message;
                pMsg->msgType = (type == "ERROR" ? CompileMessageType::Error : CompileMessageType::Warning);

                /*
                pMsg->text = string_trim(error.substr(lastBracket + 2, error.size() - lastBracket + 2));
                std::string numbers = string_trim(error.substr(bracketPos, lastBracket - bracketPos), "( )");
                auto numVec = string_split(numbers, ",");
                if (!numVec.empty())
                {
                    pMsg->line = std::max(0, std::stoi(numVec[0]) - 1);
                }
                if (numVec.size() > 1)
                {
                    auto columnVec = string_split(numVec[1], "-");
                    if (!columnVec.empty())
                    {
                        pMsg->range.first = std::max(0, std::stoi(columnVec[0]) - 1);
                        if (columnVec.size() > 1)
                        {
                            pMsg->range.second = std::stoi(columnVec[1]);
                        }
                        else
                        {
                            pMsg->range.second = pMsg->range.first + 1;
                        }
                    }
                }
                */
            }
            else
            {
                pMsg->text = error;
                pMsg->line = 0;
                pMsg->range = std::make_pair(0, 0);
                pMsg->msgType = CompileMessageType::Error;
            }
        }
        catch (...)
        {
            pMsg->text = "Failed to parse compiler error:\n" + error;
            pMsg->line = 0;
            pMsg->msgType = CompileMessageType::Error;
        }
        spResult->messages.push_back(pMsg);
    }
}
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

std::shared_ptr<CompiledShaderAssetVulkan> Compile(const vk::ShaderStageFlagBits shaderType, const fs::path& shaderPath, const std::string& shaderText)
{
    auto spResult = std::make_shared<CompiledShaderAssetVulkan>();
    spResult->path = shaderPath;
    spResult->spTags = parse_meta_tags(shaderText);
    spResult->state = CompileState::Valid;

    EShLanguage stage = GetShaderType(spResult, shaderPath);

    TBuiltInResource resource;
    init(resource);

    EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgReadHlsl);

    DirStackFileIncluder Includer;
    if (shaderPath.has_parent_path())
    {
        Includer.pushExternalLocalDirectory(shaderPath.parent_path().string());
    }

    std::string preprocessed;
    glslang::TShader shader(stage);

    //Set up Vulkan/SpirV Environment
    int clientInputSemanticsVersion = 310; // maps to, say, #define VULKAN 100
    glslang::EShTargetClientVersion VulkanClientVersion = glslang::EShTargetVulkan_1_0; // would map to, say, Vulkan 1.0
    glslang::EShTargetLanguageVersion TargetVersion = glslang::EShTargetSpv_1_0; // maps to, say, SPIR-V 1.0

    shader.setEnvInput(glslang::EShSourceHlsl, stage, glslang::EShClientVulkan, clientInputSemanticsVersion);
    shader.setEnvClient(glslang::EShClientVulkan, VulkanClientVersion);
    shader.setEnvTarget(glslang::EShTargetSpv, TargetVersion);

    const char* pInput = shaderText.c_str();
    shader.setStrings(&pInput, 1);

    std::vector<std::string> errors;
    if (!shader.preprocess(&resource, clientInputSemanticsVersion, ENoProfile, false, false, messages, &preprocessed, Includer))
    {
        LOG(INFO) << "\n" << shader.getInfoLog();
        LOG(INFO) << shader.getInfoDebugLog();

        errors.push_back(shader.getInfoLog());
        spResult->state = CompileState::Invalid;
    }
    else
    {
        const char* pProcessed = preprocessed.c_str();
        shader.setStrings(&pProcessed, 1);

        if (!shader.parse(&resource, clientInputSemanticsVersion, false, messages))
        {
            LOG(DEBUG) << "\n" << shader.getInfoLog();
            LOG(DEBUG) << shader.getInfoDebugLog();

            errors.push_back(shader.getInfoLog());
        }
        else
        {
            glslang::TProgram program;
            program.addShader(&shader);

            if (!program.link(messages))
            {
                LOG(INFO) << "\n" << shader.getInfoLog();
                LOG(INFO) << shader.getInfoDebugLog();

                errors.push_back(shader.getInfoLog());
            }
            glslang::GlslangToSpv(*program.getIntermediate(stage), spResult->spvShader);
        }
    }

    for (auto& err : errors)
    {
        ParseErrors(spResult, err);
    }

    return spResult;
}

std::shared_ptr<CompiledShaderAssetVulkan> GLSLangCompile(vk::UniqueDevice& device, vk::ShaderStageFlagBits shaderStage, const fs::path& shaderPath, std::string const& shaderText)
{
    return Compile(shaderStage, shaderPath, shaderText);
}

} // namespace Mgfx
