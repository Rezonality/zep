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

// A Helper to wrap PSO state

#include "device_dx12.h"
#include "pipeline_state.h"
#include "root_signature.h"
#include <map>
#include <thread>
#include <mutex>

using Microsoft::WRL::ComPtr;
using namespace std;

namespace Mgfx
{

GraphicsPSO::GraphicsPSO()
{
    ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
    m_PSODesc.NodeMask = 1;
    m_PSODesc.SampleMask = 0xFFFFFFFFu;
    m_PSODesc.SampleDesc.Count = 1;
    m_PSODesc.InputLayout.NumElements = 0;
}

void GraphicsPSO::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
{
    m_PSODesc.BlendState = BlendDesc;
}

void GraphicsPSO::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
{
    m_PSODesc.RasterizerState = RasterizerDesc;
}

void GraphicsPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
{
    m_PSODesc.DepthStencilState = DepthStencilDesc;
}

void GraphicsPSO::SetSampleMask(UINT SampleMask)
{
    m_PSODesc.SampleMask = SampleMask;
}

void GraphicsPSO::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE TopologyType)
{
    assert(TopologyType != D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED && "Can't draw with undefined topology");
    m_PSODesc.PrimitiveTopologyType = TopologyType;
}

void GraphicsPSO::SetPrimitiveRestart(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBProps)
{
    m_PSODesc.IBStripCutValue = IBProps;
}

void GraphicsPSO::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
    SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void GraphicsPSO::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT * RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount, UINT MsaaQuality)
{
    assert((NumRTVs == 0 || RTVFormats != nullptr) && "Null format array conflicts with non-zero length");
    for (UINT i = 0; i < NumRTVs; ++i)
        m_PSODesc.RTVFormats[i] = RTVFormats[i];
    for (UINT i = NumRTVs; i < m_PSODesc.NumRenderTargets; ++i)
        m_PSODesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
    m_PSODesc.NumRenderTargets = NumRTVs;
    m_PSODesc.DSVFormat = DSVFormat;
    m_PSODesc.SampleDesc.Count = MsaaCount;
    m_PSODesc.SampleDesc.Quality = MsaaQuality;
}

void GraphicsPSO::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC * pInputElementDescs)
{
    m_PSODesc.InputLayout.NumElements = NumElements;

    if (NumElements > 0)
    {
        D3D12_INPUT_ELEMENT_DESC* NewElements = (D3D12_INPUT_ELEMENT_DESC*)malloc(sizeof(D3D12_INPUT_ELEMENT_DESC) * NumElements);
        memcpy(NewElements, pInputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
        m_InputLayouts.reset((const D3D12_INPUT_ELEMENT_DESC*)NewElements);
    }
    else
        m_InputLayouts = nullptr;
}

void GraphicsPSO::Finalize(ID3D12Device * pDevice)
{
    // Make sure the root signature is finalized first
    m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
    assert(m_PSODesc.pRootSignature != nullptr);

    m_PSODesc.InputLayout.pInputElementDescs = nullptr;
    m_PSODesc.InputLayout.pInputElementDescs = m_InputLayouts.get();

    pDevice->CreateGraphicsPipelineState(&m_PSODesc, MY_IID_PPV_ARGS(&m_PSO));
}

void ComputePSO::Finalize(ID3D12Device * pDevice)
{
    // Make sure the root signature is finalized first
    m_PSODesc.pRootSignature = m_RootSignature->GetSignature();
    assert(m_PSODesc.pRootSignature != nullptr);

    pDevice->CreateComputePipelineState(&m_PSODesc, MY_IID_PPV_ARGS(&m_PSO));
}

ComputePSO::ComputePSO()
{
    ZeroMemory(&m_PSODesc, sizeof(m_PSODesc));
    m_PSODesc.NodeMask = 1;
}

} // Mgfx
