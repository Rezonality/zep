#include "utils.h"
#include "utils/animation/timer.h"
#include "utils/file/runtree.h"
#include "utils/string/stringutils.h"
#include "utils/ui/dpi.h"

#include "m3rdparty/threadpool/threadpool.h"
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtx/string_cast.hpp>
#include <glm/glm/gtx/transform.hpp>
#include <optional>

#include <imgui/imgui.h>
#include <imgui/examples/imgui_impl_sdl.h>
#include <imgui/examples/imgui_impl_vulkan.h>

#include "SDL_syswm.h"
#include "SDL_vulkan.h"

#include "device_vulkan.h"

#include "jorvik/editor.h"
#include "jorvik.h"
#include "meta_tags.h"

#include "visual/scene.h"
#include "visual/shader_file_asset.h"

#undef ERROR

#ifdef WIN32
HWND g_hWnd;
#endif

namespace Mgfx
{

std::map<std::string, std::string> FileNameToVkShaderType = {
    { "ps", "ps_5_0" },
    { "vs", "vs_5_0" },
    { "gs", "gs_5_0" },
    { "cs", "cs_5_0" }
};

const char* DeviceVulkan::GetName()
{
    return "Vulkan";
}

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    LOG(ERROR) << "VkResult :" << err;
    if (err < 0)
    {
        assert(!"hello");
        return;
    }
}

DeviceVulkan::DeviceVulkan()
{
}

DeviceVulkan::~DeviceVulkan()
{
    Destroy();
}

bool DeviceVulkan::Init(const char* pszWindowName)
{
    TIME_SCOPE(device_dx12_init);

    Initialized = false;

    m_pDeviceResources = std::make_unique<VkDeviceResources>(this);

    // Setup window
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    pWindow = SDL_CreateWindow(pszWindowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, jorvik.startWidth, jorvik.startHeight, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

    // Setup Platform/Renderer bindings
    ImGui_ImplSDL2_InitForVulkan(pWindow);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(pWindow, &wmInfo);

#if TARGET_PC
    // TODO: What's this for on non windows?
    g_hWnd = wmInfo.info.win.window;
#endif

    m_pDeviceResources->Init(pWindow);

    Initialized = true;

    return true;
}

void DeviceVulkan::Destroy()
{
    m_pDeviceResources->Wait();
    m_pDeviceResources->Cleanup();

    if (pWindow != nullptr)
    {
        SDL_DestroyWindow(pWindow);
        pWindow = nullptr;
    }
}

// If not found in the meta tags, then assign based on file and just use '5'
std::string GetShaderType(std::shared_ptr<CompiledShaderAssetVulkan>& spResult, const fs::path& path)
{
    std::string shaderType = spResult->spTags->shader_type.value;
    if (shaderType.empty())
    {
        for (auto& search : FileNameToVkShaderType)
        {
            if (path.stem().string() == search.first)
            {
                shaderType = search.second;
            }
        }
    }
    return shaderType;
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
// <filepath>(<linenum>,<column>-?<column>): message
// This can probably done efficiently with regex, but I'm no expert on regex,
// and it's easy to miss thing. So here we just do simple string searches
// It works, and makes up an error when it doesn't, so I can fix it!
void ParseErrors(std::shared_ptr<CompiledShaderAssetVulkan>& spResult, const std::string& output)
{
    LOG(DEBUG) << output;

    // Try to parse the DX error string into file, line, column and message
    // Exception should catch silly mistakes.
    std::vector<std::string> errors;
    string_split(output, "\n", errors);
    for (auto error : errors)
    {
        auto pMsg = std::make_shared<CompileMessage>();
        pMsg->filePath = spResult->path.string();

        try
        {
            auto bracketPos = error.find_first_of('(');
            if (bracketPos != std::string::npos)
            {
                auto lastBracket = error.find("):", bracketPos);
                if (lastBracket)
                {
                    pMsg->filePath = string_trim(error.substr(0, bracketPos));
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
                }
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

std::future<std::shared_ptr<CompileResult>> DeviceVulkan::CompileShader(const fs::path& inPath, const std::string& inText)
{
    // Copy the inputs
    fs::path path = inPath;
    std::string strText = inText;

    // Run the compiler on the threadpool
    return jorvik.spThreadPool->enqueue([this, path, strText]() {
        LOG(DEBUG) << "DX Compile thread";
        auto spResult = std::make_shared<CompiledShaderAssetVulkan>();
        spResult->path = path;
        spResult->spTags = parse_meta_tags(strText);
        //spResult->pShader = nullptr;

        /*
#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        UINT compileFlags = 0;
#endif

        auto currentRootPath = path.parent_path();
        auto fileStem = path.stem();
        auto shaderType = GetShaderType(spResult, path);
        auto entryPoint = GetEntryPoint(spResult);

        ID3D10Blob* pErrors;
        auto hr = D3DCompile(strText.c_str(), strText.size(), path.string().c_str(), NULL, NULL, entryPoint.c_str(), shaderType.c_str(), 0, 0, &spResult->pShader, &pErrors);
        if (FAILED(hr))
        {
            std::string output = std::string((const char*)pErrors->GetBufferPointer());
            LOG(DEBUG) << "Errors: " << output;
            ParseErrors(spResult, output);
            spResult->state = CompileState::Invalid;
        }
        else
        {
            Reflect(spResult, spResult->pShader.Get());
            spResult->state = CompileState::Valid;
        }
        */

        return std::dynamic_pointer_cast<CompileResult>(spResult);
    });
}

std::future<std::shared_ptr<CompileResult>> DeviceVulkan::CompilePass(PassState* renderState)
{
    // TODO: Consider repeated compile; underlying states of these objects
    // Check compiler will wait for repeated compiles of the same thing.
    return jorvik.spThreadPool->enqueue([&, renderState]()
                                            -> std::shared_ptr<CompileResult> {
        auto spResult = std::make_shared<CompiledPassRenderStateAssetVulkan>();
        spResult->state = CompileState::Invalid;

        if (renderState->vertex_shader == nullptr || renderState->pixel_shader == nullptr)
        {
            return spResult;
        }

        auto pVResult = renderState->vertex_shader->GetCompileResult();
        auto pPResult = renderState->pixel_shader->GetCompileResult();
        if (pVResult == nullptr || pPResult == nullptr || pVResult->state == CompileState::Invalid || pPResult->state == CompileState::Invalid)
        {
            return spResult;
        }

        auto spVulkanVertexShader = std::static_pointer_cast<CompiledShaderAssetVulkan>(pVResult);
        auto spVulkanPixelShader = std::static_pointer_cast<CompiledShaderAssetVulkan>(pPResult);
        if (spVulkanVertexShader == nullptr || spVulkanPixelShader == nullptr)
        {
            return spResult;
        }

        /*
        if (spVulkanVertexShader->inputLayoutDesc.empty())
        {
            return spResult;
        }

        if (spVulkanVertexShader->pShader == nullptr || spVulkanPixelShader->pShader == nullptr)
        {
            return spResult;
        }
        */

        try
        {
            /*
            LOG(DEBUG) << "Building signature & PSO on thread";
            spResult->rootSignature.Reset();
            //spResult->rootSignature.AddParameter(RootParameter::InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX));
            spResult->rootSignature.AddParameter(RootParameter::InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_PIXEL));
            HRESULT hr = spResult->rootSignature.Finalize(GetDeviceResources().GetD3DDevice(), L"Foobar", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
            if (FAILED(hr))
            {
                spResult->messages.push_back(std::make_shared<CompileMessage>(CompileMessageType::Error, renderState->vertex_shader->GetSourcePath(), GetErrorMessage(hr)));
                return spResult;
            }

            {
                D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
                psoDesc.InputLayout = {&spVulkanVertexShader->inputLayoutDesc[0], (UINT)spVulkanVertexShader->inputLayoutDesc.size()};
                psoDesc.pRootSignature = spResult->rootSignature.GetSignature();
                psoDesc.VS = CD3Vulkan_SHADER_BYTECODE(spVulkanVertexShader->pShader.Get());
                psoDesc.PS = CD3Vulkan_SHADER_BYTECODE(spVulkanPixelShader->pShader.Get());
                psoDesc.RasterizerState = CD3Vulkan_RASTERIZER_DESC(D3D12_DEFAULT);
                psoDesc.BlendState = CD3Vulkan_BLEND_DESC(D3D12_DEFAULT);
                psoDesc.DepthStencilState.DepthEnable = FALSE;
                psoDesc.DepthStencilState.StencilEnable = FALSE;
                psoDesc.SampleMask = UINT_MAX;
                psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
                psoDesc.NumRenderTargets = 1;
                psoDesc.RTVFormats[0] = GetDeviceResources().GetBackBufferFormat();
                psoDesc.SampleDesc.Count = 1;
                hr = GetDeviceResources().GetD3DDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&spResult->pPipelineState));
                if (FAILED(hr))
                {
                    // TODO:  A global error for not specific to any shader/vertex
                    if (renderState->vertex_shader->GetCompileResult()->messages.empty())
                    {
                        spResult->messages.push_back(std::make_shared<CompileMessage>(CompileMessageType::Error, renderState->vertex_shader->GetSourcePath(), GetErrorMessage(hr)));
                    }
                }
                else
                {
                    spResult->state = CompileState::Valid;
                }
            }
            LOG(DEBUG) << "Done building signature & PSO on thread";
            */
        }
        catch (...)
        {
            spResult->state = CompileState::Invalid;
        }

        spResult->spConstants = spVulkanPixelShader->spConstants;
        return spResult;
    });
}

void DeviceVulkan::BeginGUI()
{
    // Prepare the device for doing 2D Rendering using ImGUI
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL2_NewFrame(pWindow);
    ImGui::NewFrame();
}

void DeviceVulkan::EndGUI()
{
    ImGui::Render();

    auto buffer = m_pDeviceResources->GetCurrentFrame().commandBuffers[0];
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), buffer);
}

// Update the swap chain for a new client rectangle size (window sized)
void DeviceVulkan::Resize(int width, int height)
{
    m_pDeviceResources->framebufferResized = true;
}

// Handle any interesting SDL events
void DeviceVulkan::ProcessEvent(SDL_Event& event)
{
    // Just->pass->the event onto ImGUI, in case it needs mouse events, etc.
    ImGui_ImplSDL2_ProcessEvent(&event);
    if (event.type == SDL_WINDOWEVENT)
    {
        // NOTE: There is a known bug with SDL where it doesn't update the window until the size operation completes
        // Until this is fixed, you'll get an annoying stretch to the window until you finish the drag operation.
        // https://bugzilla.libsdl.org/show_bug.cgi?id=2077
        // Annoying, but not worth sweating over for now.
        if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        {
            Resize(event.window.data1, event.window.data2);
        }
    }
}

bool DeviceVulkan::RenderFrame(float frameDelta, std::function<void()> fnRenderObjects)
{
    m_pDeviceResources->Prepare();

    CheckFontUploaded();

    // Callback to render stuff
    fnRenderObjects();

    // Do the GUI rendering
    {
        BeginGUI();
        editor_draw_ui(frameDelta);
        EndGUI();
    }

    m_pDeviceResources->Present();
    return true;
}

glm::uvec2 DeviceVulkan::GetWindowSize()
{
    int w, h;
    SDL_GetWindowSize(pWindow, &w, &h);
    return glm::uvec2(w, h);
}

void DeviceVulkan::Wait()
{
    m_pDeviceResources->Wait();
}

void DeviceVulkan::DrawFSQuad(std::shared_ptr<CompileResult> state)
{
    // Use the null vertex buffer trick with a big triangle for now.
    //
    //
    /*
    auto pipeline = std::static_pointer_cast<CompiledPassRenderStateAssetVulkan>(state);
    if (pipeline == nullptr || pipeline->state != CompileState::Valid)
        return;

    auto commandList = GetDeviceResources().GetCommandList();

    commandList->SetGraphicsRootSignature(pipeline->rootSignature.GetSignature()); // set the root signature
    commandList->SetPipelineState(pipeline->pPipelineState.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); // set the primitive topology
    commandList->IASetVertexBuffers(0, 0, nullptr); 
    commandList->IASetIndexBuffer(nullptr);

    if (pipeline->spConstants != nullptr)
    {
        UpdateConstants(pipeline->spConstants);
        commandList->SetGraphicsRootConstantBufferView(0, pipeline->spConstants->pConstants->GetGPUVirtualAddress());
    }
    commandList->DrawInstanced(3, 1, 0, 0);
    */
}

void DeviceVulkan::OnDeviceLost()
{
}

void DeviceVulkan::OnDeviceRestored()
{
}

void DeviceVulkan::OnInvalidateDeviceObjects()
{
    // Ensure nothing we are removing is in the pipe
    m_pDeviceResources->Wait();

    // Get rid of ImGui stuff
    ImGui_ImplVulkan_InvalidateDeviceObjects();

    // Need to upload the texture again and update its entry in the descriptor table
    fontDirty = true;
}

void DeviceVulkan::OnCreateDeviceObjects()
{
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_pDeviceResources->instance.get();
    init_info.PhysicalDevice = m_pDeviceResources->physicalDevice;
    init_info.Device = m_pDeviceResources->device.get();
    init_info.QueueFamily = -1;
    init_info.Queue = m_pDeviceResources->graphicsQueue();
    init_info.PipelineCache = nullptr;//m_pDeviceResources->pipelineLayoutg_PipelineCache;
    init_info.DescriptorPool = m_pDeviceResources->descriptorPool;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = check_vk_result;

    // Call init, not restore, because we need to rebind the vulkan state to imgui
    ImGui_ImplVulkan_Init(&init_info, m_pDeviceResources->renderPass);
}

void DeviceVulkan::CheckFontUploaded()
{
    // Upload Fonts
    if (!ImGui::GetIO().Fonts->IsBuilt() || fontDirty)
    {
        fontDirty = false;

        // Use any command queue
        // TODO: Vulkan Noob. Not associated with a backbuffer, so does command buffer matter, since I'm resetting the pool?
        VkCommandPool commandPool = m_pDeviceResources->GetCurrentFrame().commandPool;

        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = nullptr;
        if (vkAllocateCommandBuffers(m_pDeviceResources->device.get(), &allocInfo, &commandBuffer) != VK_SUCCESS)
        {
            return;
        }

        VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        auto err = vkBeginCommandBuffer(commandBuffer, &begin_info);
        check_vk_result(err);

        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);

        VkSubmitInfo end_info = {};
        end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        end_info.commandBufferCount = 1;
        end_info.pCommandBuffers = &commandBuffer;
        err = vkEndCommandBuffer(commandBuffer);
        check_vk_result(err);

        err = vkQueueSubmit(m_pDeviceResources->graphicsQueue(), 1, &end_info, VK_NULL_HANDLE);
        check_vk_result(err);

        err = vkDeviceWaitIdle(m_pDeviceResources->device.get());
        check_vk_result(err);
        ImGui_ImplVulkan_InvalidateFontUploadObjects();
    }
}

void DeviceVulkan::OnBeginResize()
{
    // Because passstate depends on the swap chain format, and that can change
    // (aparently?!), we tear down and restore our state here
    OnInvalidateDeviceObjects();
}

void DeviceVulkan::OnEndResize()
{
    OnCreateDeviceObjects();
}

} // namespace Mgfx
