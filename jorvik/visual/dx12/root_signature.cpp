//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard
//

// A Helper to wrap root signature

#include "root_signature.h"
#include "device_dx12.h"

using namespace std;
using Microsoft::WRL::ComPtr;

namespace Mgfx
{

void RootSignature::AddStaticSampler( UINT Register, const D3D12_SAMPLER_DESC& NonStaticSamplerDesc, D3D12_SHADER_VISIBILITY Visibility)
{
    D3D12_STATIC_SAMPLER_DESC StaticSamplerDesc;

    StaticSamplerDesc.Filter = NonStaticSamplerDesc.Filter;
    StaticSamplerDesc.AddressU = NonStaticSamplerDesc.AddressU;
    StaticSamplerDesc.AddressV = NonStaticSamplerDesc.AddressV;
    StaticSamplerDesc.AddressW = NonStaticSamplerDesc.AddressW;
    StaticSamplerDesc.MipLODBias = NonStaticSamplerDesc.MipLODBias;
    StaticSamplerDesc.MaxAnisotropy = NonStaticSamplerDesc.MaxAnisotropy;
    StaticSamplerDesc.ComparisonFunc = NonStaticSamplerDesc.ComparisonFunc;
    StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    StaticSamplerDesc.MinLOD = NonStaticSamplerDesc.MinLOD;
    StaticSamplerDesc.MaxLOD = NonStaticSamplerDesc.MaxLOD;
    StaticSamplerDesc.ShaderRegister = Register;
    StaticSamplerDesc.RegisterSpace = 0;
    StaticSamplerDesc.ShaderVisibility = Visibility;

    if (StaticSamplerDesc.AddressU == D3D12_TEXTURE_ADDRESS_MODE_BORDER || StaticSamplerDesc.AddressV == D3D12_TEXTURE_ADDRESS_MODE_BORDER || StaticSamplerDesc.AddressW == D3D12_TEXTURE_ADDRESS_MODE_BORDER)
    {
        /*
        WARN_ONCE_IF_NOT(
            // Transparent Black
            NonStaticSamplerDesc.BorderColor[0] == 0.0f &&
            NonStaticSamplerDesc.BorderColor[1] == 0.0f &&
            NonStaticSamplerDesc.BorderColor[2] == 0.0f &&
            NonStaticSamplerDesc.BorderColor[3] == 0.0f ||
            // Opaque Black
            NonStaticSamplerDesc.BorderColor[0] == 0.0f &&
            NonStaticSamplerDesc.BorderColor[1] == 0.0f &&
            NonStaticSamplerDesc.BorderColor[2] == 0.0f &&
            NonStaticSamplerDesc.BorderColor[3] == 1.0f ||
            // Opaque White
            NonStaticSamplerDesc.BorderColor[0] == 1.0f &&
            NonStaticSamplerDesc.BorderColor[1] == 1.0f &&
            NonStaticSamplerDesc.BorderColor[2] == 1.0f &&
            NonStaticSamplerDesc.BorderColor[3] == 1.0f,
            "Sampler border color does not match static sampler limitations");
            */

        if (NonStaticSamplerDesc.BorderColor[3] == 1.0f)
        {
            if (NonStaticSamplerDesc.BorderColor[0] == 1.0f)
                StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
            else
                StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
        }
        else
            StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    }
    m_samplers.push_back(StaticSamplerDesc);
}

HRESULT RootSignature::Finalize(ID3D12Device * pDevice, const std::wstring & name, D3D12_ROOT_SIGNATURE_FLAGS Flags)
{
    D3D12_ROOT_SIGNATURE_DESC RootDesc;

    // params
    RootDesc.NumParameters = (UINT)m_params.size();
    if (!m_params.empty())
    {
        RootDesc.pParameters = (const D3D12_ROOT_PARAMETER*)&m_params[0];
    }

    // static samplers
    RootDesc.NumStaticSamplers = (UINT)m_samplers.size();
    if (!m_samplers.empty())
    {
        RootDesc.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)&m_samplers[0];
    }
    else
    {
        RootDesc.pStaticSamplers = nullptr;
    }
    RootDesc.Flags = Flags;

    m_DescriptorTableBitMap = 0;
    m_SamplerTableBitMap = 0;

    for (UINT Param = 0; Param < (UINT)m_params.size(); ++Param)
    {
        const D3D12_ROOT_PARAMETER& RootParam = RootDesc.pParameters[Param];
        m_DescriptorTableSize[Param] = 0;

        if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            assert(RootParam.DescriptorTable.pDescriptorRanges != nullptr);

            // We keep track of sampler descriptor tables separately from CBV_SRV_UAV descriptor tables
            if (RootParam.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
            {
                m_SamplerTableBitMap |= (1 << Param);
            }
            else
            {
                m_DescriptorTableBitMap |= (1 << Param);
            }

            for (UINT TableRange = 0; TableRange < RootParam.DescriptorTable.NumDescriptorRanges; ++TableRange)
            {
                m_DescriptorTableSize[Param] += RootParam.DescriptorTable.pDescriptorRanges[TableRange].NumDescriptors;
            }
        }
    }

    ComPtr<ID3DBlob> pOutBlob, pErrorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1, pOutBlob.GetAddressOf(), pErrorBlob.GetAddressOf());
    if (SUCCEEDED(hr))
    {
        hr = pDevice->CreateRootSignature(1, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), MY_IID_PPV_ARGS(m_spSignature.GetAddressOf()));
        if (SUCCEEDED(hr))
        {
            m_spSignature->SetName(name.c_str());
        }
    }
    return hr;
}

void RootSignature::Reset()
{
    m_spSignature.Reset();

    for (auto& param : m_params)
    {
        // Ensure parameter memory for descriptors is cleared
        param.Clear();
    }
    m_params.clear();

    m_samplers.clear();
}

void RootSignature::AddParameter(const RootParameter& param)
{
    m_params.push_back(param);
}

void RootSignature::AddSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
{
    m_samplers.push_back(sampler);
}

ID3D12RootSignature* RootSignature::GetSignature() const
{
    return m_spSignature.Get();
}

} // Mgfx
