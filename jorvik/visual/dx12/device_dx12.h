#pragma once

#ifdef WIN32
#include "d3d12.h"
#include <dxgi1_4.h>

#include <glm/glm.hpp>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

//#include "jorvik/visual/mesh.h"

#define MY_IID_PPV_ARGS IID_PPV_ARGS
#define D3D12_GPU_VIRTUAL_ADDRESS_NULL ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN ((D3D12_GPU_VIRTUAL_ADDRESS)-1)

#include <wrl.h>
#include <d3dcompiler.h>

#include "device_resources.h"
#include "root_signature.h"

#include "utils/logger.h"

#include "compile.h"
#include "visual/IDevice.h"

struct SDL_Window;
union SDL_Event;

namespace Mgfx
{

class DeviceDX12;
struct PassState;
struct ConstantsDX12;
class CompiledShaderAssetDX12 : public CompileResult
{
public:
    virtual ~CompiledShaderAssetDX12()
    {
        LOG(DEBUG) << "Destroyed a ShaderAssetDX12";
    }
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutDesc;
    Microsoft::WRL::ComPtr<ID3D10Blob> pShader;
    std::vector<std::string> semantics;
    std::shared_ptr<ConstantsDX12> spConstants;
};

struct CompiledPassRenderStateAssetDX12 : CompileResult
{
    virtual ~CompiledPassRenderStateAssetDX12()
    {
        LOG(DEBUG) << "Destroyed a PassRenderStateAssetDX12";
    }

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipelineState;
    RootSignature rootSignature;
    std::shared_ptr<ConstantsDX12> spConstants;
};

struct DeviceMaterial
{
    D3D12_CPU_DESCRIPTOR_HANDLE SRVs[6];
};

class DeviceDX12 : public IDevice, public DX::IDeviceNotify
{
public:
    DeviceDX12();
    ~DeviceDX12();

    virtual bool Init(const char* pszWindowName) override;
    virtual void Destroy() override;
    virtual bool RenderFrame(float frameDelta, std::function<void()> fnRenderObjects) override;
    virtual void BeginGUI() override;
    virtual void EndGUI() override;
    virtual void Resize(int width, int height) override;
    virtual void ProcessEvent(SDL_Event& event) override;
    virtual const char* GetName() override;
    virtual glm::uvec2 GetWindowSize() override;
    virtual void Wait() override;

    virtual std::future<std::shared_ptr<CompileResult>> CompileShader(const fs::path& path, const std::string& strText) override;
    virtual std::future<std::shared_ptr<CompileResult>> CompilePass(PassState* pPassRenderState) override;

    virtual void DrawFSQuad(std::shared_ptr<CompileResult> state) override;

    // Inherited via IDeviceNotify
    virtual void OnDeviceLost() override;
    virtual void OnDeviceRestored() override;

    DX::DeviceResources& GetDeviceResources() const
    {
        return *m_deviceResources;
    }

private:
    void Clear();
    void CreateVertexInputLayout(std::shared_ptr<CompiledShaderAssetDX12> spResult, ID3D10Blob* pBlob, ID3D12ShaderReflection* pReflection);
    void Reflect(std::shared_ptr<CompiledShaderAssetDX12> spResult, ID3D10Blob* pBlob);

private:
    std::unique_ptr<DX::DeviceResources> m_deviceResources;
};

} // namespace Mgfx

#endif
