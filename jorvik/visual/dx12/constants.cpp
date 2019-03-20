#include "Constants.h"
#include "utils/logger.h"

using namespace Microsoft::WRL;

namespace Mgfx
{

bool CreateConstantBufferMemory(DeviceDX12& device, std::shared_ptr<ConstantsDX12> spConstants)
{
    auto d3dDevice = device.GetDeviceResources().GetD3DDevice();
    
    // Create the constant buffer memory and map the CPU and GPU addresses
    const D3D12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

    // Allocate one constant buffer per frame, since it gets updated every frame.
    size_t cbSize = sizeof(Constants);
    const D3D12_RESOURCE_DESC constantBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    HRESULT hr = d3dDevice->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &constantBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&spConstants->pConstants));
    if (FAILED(hr))
    {
        return false;
    }

    // Map the constant buffer and cache its heap pointers.
    // We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
    CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
    hr = spConstants->pConstants->Map(0, nullptr, reinterpret_cast<void**>(&spConstants->mappedConstantData));
    if (FAILED(hr))
    {
        spConstants->pConstants.Reset();
        return false;
    }
    return true;
}

void UpdateConstants(std::shared_ptr<ConstantsDX12>& spConstants)
{
    auto pMem = spConstants->mappedConstantData;
    assert(pMem);

    *(float*)pMem = jorvik.time;
}

std::shared_ptr<ConstantsDX12> BuildConstantBuffer(DeviceDX12& device, ID3D12ShaderReflectionConstantBuffer* pConstantBuffer)
{
    auto spConstants = std::make_shared<ConstantsDX12>();

    D3D12_SHADER_BUFFER_DESC bufferDesc;
    pConstantBuffer->GetDesc(&bufferDesc);

    for (UINT param = 0; param < bufferDesc.Variables; param++)
    {
        auto pVar = pConstantBuffer->GetVariableByIndex(param);

        D3D12_SHADER_VARIABLE_DESC varDesc;
        pVar->GetDesc(&varDesc);

        LOG(DEBUG) << "    Constant Var: " << varDesc.Name;
    }

    if (CreateConstantBufferMemory(device, spConstants))
    {
        return spConstants;
    }

    return nullptr;
}

} // namespace Mgfx
