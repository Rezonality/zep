#pragma once

#include "device_dx12.h"

namespace Mgfx
{

struct MyConstants
{
    float time;
};

static_assert(sizeof(MyConstants) < D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, "Checking the size here.");
union Constants
{
    MyConstants constants;
    uint8_t alignmentPadding[D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT];
};

struct ConstantsDX12
{
    std::vector<uint8_t> data;
    Constants*  mappedConstantData;
    Microsoft::WRL::ComPtr<ID3D12Resource> pConstants;
};

std::shared_ptr<ConstantsDX12> BuildConstantBuffer(DeviceDX12& device, ID3D12ShaderReflectionConstantBuffer* pConstants);
void UpdateConstants(std::shared_ptr<ConstantsDX12>& spConstants);

}
