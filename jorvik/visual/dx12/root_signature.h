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
// Adapted for Jorvik.
// Threading/caching stuff removed for now, since that is more of a global issue for jorvik, and 
// somewhat less important for the use case.

#pragma once

#include "d3d12.h"
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <wrl.h>

namespace Mgfx
{

class RootParameter
{
public:
    void Clear()
    {
        if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            delete[] param.DescriptorTable.pDescriptorRanges;
    }

    static RootParameter InitAsConstants(UINT Register, UINT NumDwords, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
    {
        RootParameter p;
        p.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        p.param.ShaderVisibility = Visibility;
        p.param.Constants.Num32BitValues = NumDwords;
        p.param.Constants.ShaderRegister = Register;
        p.param.Constants.RegisterSpace = 0;
        return p;
    }

    static RootParameter InitAsConstantBuffer(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
    {
        RootParameter p;
        p.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        p.param.ShaderVisibility = Visibility;
        p.param.Descriptor.ShaderRegister = Register;
        p.param.Descriptor.RegisterSpace = 0;
        return p;
    }

    static RootParameter InitAsBufferSRV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
    {
        RootParameter p;
        p.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        p.param.ShaderVisibility = Visibility;
        p.param.Descriptor.ShaderRegister = Register;
        p.param.Descriptor.RegisterSpace = 0;
        return p;
    }

    static RootParameter InitAsBufferUAV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
    {
        RootParameter p;
        p.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        p.param.ShaderVisibility = Visibility;
        p.param.Descriptor.ShaderRegister = Register;
        p.param.Descriptor.RegisterSpace = 0;
        return p;
    }

    static RootParameter InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
    {
        RootParameter p;
        p = InitAsDescriptorTable(1, Visibility);
        SetTableRange(p.param, 0, Type, Register, Count);
        return p;
    }

    static RootParameter InitAsDescriptorTable(UINT RangeCount, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
    {
        RootParameter p;
        p.param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        p.param.ShaderVisibility = Visibility;
        p.param.DescriptorTable.NumDescriptorRanges = RangeCount;
        p.param.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[RangeCount];
        return p;
    }

    static void SetTableRange(D3D12_ROOT_PARAMETER& param, UINT RangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, UINT Space = 0)
    {
        D3D12_DESCRIPTOR_RANGE* range = const_cast<D3D12_DESCRIPTOR_RANGE*>(param.DescriptorTable.pDescriptorRanges + RangeIndex);
        range->RangeType = Type;
        range->NumDescriptors = Count;
        range->BaseShaderRegister = Register;
        range->RegisterSpace = Space;
        range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    }
    D3D12_ROOT_PARAMETER param;
};

// Maximum 64 DWORDS divied up amongst all root parameters.
// Root constants = 1 DWORD * NumConstants
// Root descriptor (CBV, SRV, or UAV) = 2 DWORDs each
// Descriptor table pointer = 1 DWORD
// Static samplers = 0 DWORDS (compiled into shader)
class RootSignature
{
public:
    RootSignature()
    {
        Reset();
    }

    ~RootSignature()
    {
    }

    void Reset();
    HRESULT Finalize(ID3D12Device* pDevice, const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);
    ID3D12RootSignature* GetSignature() const;
    void AddParameter(const RootParameter& param);
    void AddSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);
    void AddStaticSampler(UINT Register, const D3D12_SAMPLER_DESC& NonStaticSamplerDesc, D3D12_SHADER_VISIBILITY Visibility);
    
protected:
    uint32_t m_DescriptorTableBitMap; // One bit is set for root parameters that are non-sampler descriptor tables
    uint32_t m_SamplerTableBitMap; // One bit is set for root parameters that are sampler descriptor tables
    uint32_t m_DescriptorTableSize[16]; // Non-sampler descriptor tables need to know their descriptor count
    std::vector<RootParameter> m_params;
    std::vector<D3D12_STATIC_SAMPLER_DESC> m_samplers;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_spSignature;
};

} // namespace Mgfx
