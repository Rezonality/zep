#include "utils.h"
#include "utils/file/runtree.h"
#include "utils/animation/timer.h"
#include "utils/string/stringutils.h"
#include "utils/ui/dpi.h"

#include "m3rdparty/threadpool/threadpool.h"
#include <glm/glm/gtc/matrix_transform.hpp>
#include <glm/glm/gtx/string_cast.hpp>
#include <glm/glm/gtx/transform.hpp>

#include <imgui/imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_sdl.h>

#include "SDL_syswm.h"

#include "device_dx12.h"
#include "constants.h"

#include "jorvik/editor.h"
#include "jorvik.h"
#include "meta_tags.h"

#include "visual/shader_file_asset.h"
#include "visual/scene.h"


#include "root_signature.h"

#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
#endif

#undef ERROR

extern HWND g_hWnd;
using namespace Microsoft::WRL;
using namespace DirectX;

namespace Mgfx
{

std::string GetErrorMessage(HRESULT hr)
{
    LPSTR s;
    if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
            NULL, // source ignored for FROM_SYSTEM
            hr, // error code
            0, // default language ID
            (LPSTR)&s, // pointer to pointer to buffer
            0, // Minimum buffer length
            NULL) // arg array
        == 0)
    {
        return std::string("Unknown error");
    }
    std::string strMessage(s);
    LocalFree(s);
    return s;
}

std::map<std::string, std::string> FileNameToShaderType = {
    { "ps", "ps_5_0" },
    { "vs", "vs_5_0" },
    { "gs", "gs_5_0" },
    { "cs", "cs_5_0" }
};

const char* DeviceDX12::GetName()
{
    return "DX12";
}

bool DeviceDX12::Init(const char* pszWindowName)
{
    TIME_SCOPE(device_dx12_init);

    Initialized = false;

    // Setup window
    SDL_DisplayMode current;
    SDL_GetCurrentDisplayMode(0, &current);
    pWindow = SDL_CreateWindow(pszWindowName, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, jorvik.startWidth, jorvik.startHeight, SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(pWindow, &wmInfo);

    g_hWnd = wmInfo.info.win.window;

    int width, height;
    SDL_GetWindowSize(pWindow, &width, &height);
    LOG(INFO) << "Window: " << width << ", " << height;

    m_deviceResources = std::make_unique<DxDeviceResources>(this, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_D32_FLOAT, 2, D3D_FEATURE_LEVEL_11_0, 0);// DX::DxDeviceResources::c_AllowTearing);
    m_deviceResources->SetWindow(g_hWnd, width, height);
    m_deviceResources->CreateDeviceResources();
    m_deviceResources->CreateWindowSizeDependentResources();

    ImGui_ImplDX12_Init(m_deviceResources->GetD3DDevice(),
        m_deviceResources->GetBackBufferCount(),
        m_deviceResources->GetBackBufferFormat(),
        m_deviceResources->GetFontHeapCPUView(),
        m_deviceResources->GetFontHeapGPUView());

    ImGui_ImplSDL2_Init(pWindow);

    Initialized = true;

    return true;
}

DeviceDX12::DeviceDX12()
{
}

DeviceDX12::~DeviceDX12()
{
    Destroy();
}

void DeviceDX12::Destroy()
{
    if (m_deviceResources)
    {
        m_deviceResources->WaitForGpu();
    }

    ImGui_ImplDX12_Shutdown();

    if (pWindow != nullptr)
    {
        SDL_DestroyWindow(pWindow);
        pWindow = nullptr;
    }

    m_deviceResources.reset();
}

// Used by the compiler to resolve include files; not yet used in Jorvik
HRESULT Includer(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* ppData, UINT* pBytes)
{
    auto inc = runtree_load_asset(fs::path(pFileName));
    if (inc.empty())
    {
        LOG(ERROR) << "#include: " << pFileName << " failed!";
        return E_FAIL;
    }

    auto pMem = new uint8_t[inc.size()];
    memcpy(pMem, inc.c_str(), inc.size());
    *ppData = pMem;
    *pBytes = UINT(inc.size());
    return S_OK;
}

// If not found in the meta tags, then assign based on file and just use '5'
std::string GetShaderType(std::shared_ptr<CompiledShaderAssetDX12>& spResult, const fs::path& path)
{
    std::string shaderType = spResult->spTags->shader_type.value;
    if (shaderType.empty())
    {
        for (auto& search : FileNameToShaderType)
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
std::string GetEntryPoint(std::shared_ptr<CompiledShaderAssetDX12>& spResult)
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
void ParseErrors(std::shared_ptr<CompiledShaderAssetDX12>& spResult, const std::string& output)
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

void DeviceDX12::Reflect(std::shared_ptr<CompiledShaderAssetDX12> spResult, ID3D10Blob* pBlob)
{
    LOG(DEBUG) << "Reflecting...";
    ComPtr<ID3D12ShaderReflection> reflect;
    HRESULT hr = D3DReflect(pBlob->GetBufferPointer(), pBlob->GetBufferSize(), IID_PPV_ARGS(reflect.GetAddressOf()));
    if (FAILED(hr))
    {
        // Reflect error?
    }

    D3D12_SHADER_DESC desc;
    hr = reflect->GetDesc(&desc);
    if (FAILED(hr))
    {
        // err
    }

    LOG(DEBUG) << "Input Params: " << desc.InputParameters;
    for (UINT param = 0; param < desc.InputParameters; param++)
    {
        D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
        hr = reflect->GetInputParameterDesc(param, &paramDesc);
        LOG(DEBUG) << "    Input Param: " << paramDesc.SemanticName;
    }

    LOG(DEBUG) << "Constant Buffers: " << desc.ConstantBuffers;
    for (UINT buffer = 0; buffer < desc.ConstantBuffers; buffer++)
    {
        auto pConstantBuffer = reflect->GetConstantBufferByIndex(buffer);
        spResult->spConstants = BuildConstantBuffer(*this, pConstantBuffer);
    }

    LOG(DEBUG) << "Resource Bindings: " << desc.BoundResources;
    for (UINT resource = 0; resource < desc.BoundResources; resource++)
    {
        D3D12_SHADER_INPUT_BIND_DESC bindDesc;
        reflect->GetResourceBindingDesc(resource, &bindDesc);

        LOG(DEBUG) << "    Binding: " << bindDesc.Name;
    }

    auto pos = spResult->spTags->shader_type.value.find("vs", 0);
    if (pos == 0)
    {
        CreateVertexInputLayout(spResult, pBlob, reflect.Get());
    }
};

void DeviceDX12::CreateVertexInputLayout(std::shared_ptr<CompiledShaderAssetDX12> spResult, ID3D10Blob* pBlob, ID3D12ShaderReflection* pReflection)
{
    LOG(DEBUG) << "CreateVertexInputLayout";
    D3D12_SHADER_DESC desc;
    HRESULT hr = pReflection->GetDesc(&desc);
    if (FAILED(hr))
    {
        return;
    }

    for (uint32_t i = 0; i < desc.InputParameters; i++)
    {
        D3D12_SIGNATURE_PARAMETER_DESC paramDesc;
        pReflection->GetInputParameterDesc(i, &paramDesc);

        spResult->semantics.push_back(paramDesc.SemanticName);
        D3D12_INPUT_ELEMENT_DESC elementDesc;
        elementDesc.SemanticName = spResult->semantics.back().c_str();
        elementDesc.SemanticIndex = paramDesc.SemanticIndex;
        elementDesc.InputSlot = 0;
        elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        elementDesc.InstanceDataStepRate = 0;
        elementDesc.AlignedByteOffset = i == 0 ? 0 : D3D12_APPEND_ALIGNED_ELEMENT;

        if (paramDesc.Mask == 1)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                elementDesc.Format = DXGI_FORMAT_R32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format = DXGI_FORMAT_R32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format = DXGI_FORMAT_R32_FLOAT;
        }
        else if (paramDesc.Mask <= 3)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                elementDesc.Format = DXGI_FORMAT_R32G32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format = DXGI_FORMAT_R32G32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
        }
        else if (paramDesc.Mask <= 7)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                elementDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
        }
        else if (paramDesc.Mask <= 15)
        {
            if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)
                elementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)
                elementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
            else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32)
                elementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        }

        spResult->inputLayoutDesc.push_back(elementDesc);
    }
}

std::future<std::shared_ptr<CompileResult>> DeviceDX12::CompileShader(const fs::path& inPath, const std::string& inText)
{
    // Copy the inputs
    fs::path path = inPath;
    std::string strText = inText;

    // Run the compiler on the threadpool
    return jorvik.spThreadPool->enqueue([this, path, strText]() {
        LOG(DEBUG) << "DX Compile thread";
        auto spResult = std::make_shared<CompiledShaderAssetDX12>();
        spResult->path = path;
        spResult->spTags = parse_meta_tags(strText);
        spResult->pShader = nullptr;

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

        return std::dynamic_pointer_cast<CompileResult>(spResult);
    });
}

std::future<std::shared_ptr<CompileResult>> DeviceDX12::CompilePass(PassState* renderState)
{
    // TODO: Consider repeated compile; underlying states of these objects
    // Check compiler will wait for repeated compiles of the same thing.
    return jorvik.spThreadPool->enqueue([&, renderState]()
        -> std::shared_ptr<CompileResult>
    {

        auto spResult = std::make_shared<CompiledPassRenderStateAssetDX12>();
        spResult->state = CompileState::Invalid;

        if (renderState->vertex_shader == nullptr || renderState->pixel_shader == nullptr)
        {
            return spResult;
        }

        auto pVResult = renderState->vertex_shader->GetCompileResult();
        auto pPResult = renderState->pixel_shader->GetCompileResult();
        if (pVResult == nullptr || pPResult == nullptr ||
            pVResult->state == CompileState::Invalid ||
            pPResult->state == CompileState::Invalid)
        {
            return spResult;
        }


        auto spDX12VertexShader = std::static_pointer_cast<CompiledShaderAssetDX12>(pVResult);
        auto spDX12PixelShader = std::static_pointer_cast<CompiledShaderAssetDX12>(pPResult);
        if (spDX12VertexShader == nullptr || spDX12PixelShader == nullptr)
        {
            return spResult;
        }

        if (spDX12VertexShader->inputLayoutDesc.empty())
        {
            return spResult;
        }

        if (spDX12VertexShader->pShader == nullptr || spDX12PixelShader->pShader == nullptr)
        {
            return spResult;
        }

        try
        {
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
                psoDesc.InputLayout = {&spDX12VertexShader->inputLayoutDesc[0], (UINT)spDX12VertexShader->inputLayoutDesc.size()};
                psoDesc.pRootSignature = spResult->rootSignature.GetSignature();
                psoDesc.VS = CD3DX12_SHADER_BYTECODE(spDX12VertexShader->pShader.Get());
                psoDesc.PS = CD3DX12_SHADER_BYTECODE(spDX12PixelShader->pShader.Get());
                psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
                psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
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
        }
        catch (...)
        {
            spResult->state = CompileState::Invalid;
        }

        spResult->spConstants = spDX12PixelShader->spConstants;
        return spResult;
    });
}

void DeviceDX12::BeginGUI()
{
    // Prepare the device for doing 2D Rendering using ImGUI
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplSDL2_NewFrame(pWindow);
    ImGui::NewFrame();
}

void DeviceDX12::EndGUI()
{
    ImGui::Render();

    m_deviceResources->GetCommandList()->SetDescriptorHeaps(1, m_deviceResources->GetFontHeap());
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_deviceResources->GetCommandList());
}

// Update the swap chain for a new client rectangle size (window sized)
void DeviceDX12::Resize(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;
}

// Handle any interesting SDL events
void DeviceDX12::ProcessEvent(SDL_Event& event)
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

bool DeviceDX12::RenderFrame(float frameDelta, std::function<void()> fnRenderObjects)
{
    m_deviceResources->Prepare();

    Clear();

    // Callback to render stuff
    fnRenderObjects();

    // Do the GUI rendering
    {
        BeginGUI();
        editor_draw_ui(frameDelta);
        EndGUI();
    }

    m_deviceResources->Present();
    return true;
}

// Helper method to clear the back buffers.
void DeviceDX12::Clear()
{
    auto commandList = m_deviceResources->GetCommandList();

    // Clear the views.
    auto rtvDescriptor = m_deviceResources->GetRenderTargetView();
    auto dsvDescriptor = m_deviceResources->GetDepthStencilView();

    commandList->OMSetRenderTargets(1, &rtvDescriptor, FALSE, &dsvDescriptor);
    commandList->ClearRenderTargetView(rtvDescriptor, Colors::CornflowerBlue, 0, nullptr);
    commandList->ClearDepthStencilView(dsvDescriptor, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set the viewport and scissor rect.
    auto viewport = m_deviceResources->GetScreenViewport();
    auto scissorRect = m_deviceResources->GetScissorRect();
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissorRect);
}

glm::uvec2 DeviceDX12::GetWindowSize()
{
    int w, h;
    SDL_GetWindowSize(pWindow, &w, &h);
    return glm::uvec2(w, h);
}

void DeviceDX12::OnDeviceLost()
{
}

void DeviceDX12::OnDeviceRestored()
{
}

void DeviceDX12::OnInvalidateDeviceObjects()
{
    ImGui_ImplDX12_InvalidateDeviceObjects();
}

void DeviceDX12::OnCreateDeviceObjects()
{
    ImGui_ImplDX12_CreateDeviceObjects();
}

void DeviceDX12::OnBeginResize()
{
}

void DeviceDX12::OnEndResize()
{
}

void DeviceDX12::Wait()
{
    m_deviceResources->WaitForGpu();

}
void DeviceDX12::DrawFSQuad(std::shared_ptr<CompileResult> state)
{
    // Use the null vertex buffer trick with a big triangle for now.
    auto pipeline = std::static_pointer_cast<CompiledPassRenderStateAssetDX12>(state);
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
}

} // namespace Mgfx

// Temporary
/*
void DeviceDX12::CreateGeometry()
{
    // Create an empty root signature.
    {
    }


    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(m_commandList->Close());

    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.
        ThrowIfFailed(m_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}
*/

/*
struct Vertex {
    Vertex(float x, float y, float z, float r, float g, float b, float a) : pos(x, y, z), color(r, g, b, z) {}
    XMFLOAT3 pos;
    XMFLOAT4 color;
};

    // Create vertex buffer

    // a triangle
    Vertex vList[] = {
        // first quad (closer to camera, blue)
        { -0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        {  0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        { -0.5f, -0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },
        {  0.5f,  0.5f, 0.5f, 0.0f, 0.0f, 1.0f, 1.0f },

        // second quad (further from camera, green)
        { -0.75f,  0.75f,  0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
        {   0.0f,  0.0f, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
        { -0.75f,  0.0f, 0.7f, 0.0f, 1.0f, 0.0f, 1.0f },
        {   0.0f,  0.75f,  0.7f, 0.0f, 1.0f, 0.0f, 1.0f }
    };

    int vBufferSize = sizeof(vList);

    // create default heap
    // default heap is memory on the GPU. Only the GPU has access to this memory
    // To get data into this heap, we will have to upload the data using
    // an upload heap
    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_COPY_DEST, // we will start this heap in the copy destination state since we will copy data
                                        // from the upload heap to this heap
        nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
        IID_PPV_ARGS(&vertexBuffer));

    // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
    vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

    // create upload heap
    // upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
    // We will upload the vertex buffer using this heap to the default heap
    ID3D12Resource* vBufferUploadHeap;
    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
        nullptr,
        IID_PPV_ARGS(&vBufferUploadHeap));
    vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

    // store vertex buffer in upload heap
    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = reinterpret_cast<BYTE*>(vList); // pointer to our vertex array
    vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
    vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data

    // we are now creating a command with the command list to copy the data from
    // the upload heap to the default heap
    UpdateSubresources(commandList, vertexBuffer, vBufferUploadHeap, 0, 0, 1, &vertexData);

    // transition the vertex buffer data from copy destination state to vertex buffer state
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    // Create index buffer

    // a quad (2 triangles)
    DWORD iList[] = {
        // first quad (blue)
        0, 1, 2, // first triangle
        0, 3, 1, // second triangle
    };

    int iBufferSize = sizeof(iList);

    // create default heap to hold index buffer
    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), // a default heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(iBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_COPY_DEST, // start in the copy destination state
        nullptr, // optimized clear value must be null for this type of resource
        IID_PPV_ARGS(&indexBuffer));

    // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
    vertexBuffer->SetName(L"Index Buffer Resource Heap");

    // create upload heap to upload index buffer
    ID3D12Resource* iBufferUploadHeap;
    device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), // upload heap
        D3D12_HEAP_FLAG_NONE, // no flags
        &CD3DX12_RESOURCE_DESC::Buffer(vBufferSize), // resource description for a buffer
        D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
        nullptr,
        IID_PPV_ARGS(&iBufferUploadHeap));
    vBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

    // store vertex buffer in upload heap
    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = reinterpret_cast<BYTE*>(iList); // pointer to our index array
    indexData.RowPitch = iBufferSize; // size of all our index buffer
    indexData.SlicePitch = iBufferSize; // also the size of our index buffer

    // we are now creating a command with the command list to copy the data from
    // the upload heap to the default heap
    UpdateSubresources(commandList, indexBuffer, iBufferUploadHeap, 0, 0, 1, &indexData);

    // transition the vertex buffer data from copy destination state to vertex buffer state
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));

    // Now we execute the command list to upload the initial assets (triangle data)
    commandList->Close();
    ID3D12CommandList* ppCommandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
    fenceValue[frameIndex]++;
    hr = commandQueue->Signal(fence[frameIndex], fenceValue[frameIndex]);
    if (FAILED(hr))
    {
        Running = false;
    }

    // create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vBufferSize;

    // create a vertex buffer view for the triangle. We get the GPU memory address to the vertex pointer using the GetGPUVirtualAddress() method
    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32-bit unsigned integer (this is what a dword is, double word, a word is 2 bytes)
    indexBufferView.SizeInBytes = iBufferSize;

    return true;
}

void Cleanup()
{
    SAFE_RELEASE(pipelineStateObject);
    SAFE_RELEASE(rootSignature);
    SAFE_RELEASE(vertexBuffer);
    SAFE_RELEASE(indexBuffer);
}

*/
