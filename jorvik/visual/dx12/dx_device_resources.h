//
// DxDeviceResources.h - A wrapper for the Direct3D 11 device and swapchain
//

#pragma once

#include <WinSDKVer.h>
#define _WIN32_WINNT 0x0A00
#include <SDKDDKVer.h>

// Use the C++ standard templated min/max
#define NOMINMAX

// DirectX apps don't need GDI
#define NODRAWTEXT
#define NOGDI
#define NOBITMAP

// Include <mcx.h> if you need this
#define NOMCX

// Include <winsvc.h> if you need this
#define NOSERVICE

// WinHelp is deprecated
#define NOHELP

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <wrl/client.h>
#include <wrl/event.h>

#include <d3d12.h>

#if defined(NTDDI_WIN10_RS2)
#include <dxgi1_6.h>
#else
#include <dxgi1_5.h>
#endif

#include <DirectXColors.h>
#include <DirectXMath.h>

#include "d3dx12.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>

#include <stdio.h>

// To use graphics markup events with the latest version of PIX, change this to include <pix3.h>
// then add the NuGet package WinPixEventRuntime to the project.
#include <pix.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif
#include "visual/IDevice.h"

namespace Mgfx
{

// Helper class for COM exceptions
class com_exception : public std::exception
{
public:
    com_exception(HRESULT hr)
        : result(hr)
    {
    }

    virtual const char* what() const override
    {
        static char s_str[64] = {};
        sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
        return s_str;
    }

private:
    HRESULT result;
};

// Helper utility converts D3D API failures into exceptions.
inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        throw com_exception(hr);
    }
}

// Controls all the DirectX device resources.
class DxDeviceResources
{
public:
    static const unsigned int c_AllowTearing = 0x1;
    static const unsigned int c_EnableHDR = 0x2;

    DxDeviceResources(Mgfx::IDeviceNotify* pNotify, DXGI_FORMAT backBufferFormat = DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_FORMAT depthBufferFormat = DXGI_FORMAT_D32_FLOAT,
        UINT backBufferCount = 2,
        D3D_FEATURE_LEVEL minFeatureLevel = D3D_FEATURE_LEVEL_11_0,
        unsigned int flags = 0) noexcept(false);
    ~DxDeviceResources();

    void CreateDeviceResources();
    void CreateWindowSizeDependentResources();
    void SetWindow(HWND window, int width, int height);
    bool WindowSizeChanged(int width, int height);
    void HandleDeviceLost();
    void Prepare(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_PRESENT);
    void Present(D3D12_RESOURCE_STATES beforeState = D3D12_RESOURCE_STATE_RENDER_TARGET);
    void WaitForGpu() noexcept;

    // Device Accessors.
    RECT GetOutputSize() const
    {
        return m_outputSize;
    }

    // Direct3D Accessors.
    ID3D12Device* GetD3DDevice() const
    {
        return m_d3dDevice.Get();
    }
    IDXGISwapChain3* GetSwapChain() const
    {
        return m_swapChain.Get();
    }
    IDXGIFactory4* GetDXGIFactory() const
    {
        return m_dxgiFactory.Get();
    }
    D3D_FEATURE_LEVEL GetDeviceFeatureLevel() const
    {
        return m_d3dFeatureLevel;
    }
    ID3D12Resource* GetRenderTarget() const
    {
        return m_renderTargets[m_backBufferIndex].Get();
    }
    ID3D12Resource* GetDepthStencil() const
    {
        return m_depthStencil.Get();
    }
    ID3D12CommandQueue* GetCommandQueue() const
    {
        return m_commandQueue.Get();
    }
    ID3D12CommandAllocator* GetCommandAllocator() const
    {
        return m_commandAllocators[m_backBufferIndex].Get();
    }
    ID3D12GraphicsCommandList* GetCommandList() const
    {
        return m_commandList.Get();
    }
    DXGI_FORMAT GetBackBufferFormat() const
    {
        return m_backBufferFormat;
    }
    DXGI_FORMAT GetDepthBufferFormat() const
    {
        return m_depthBufferFormat;
    }
    D3D12_VIEWPORT GetScreenViewport() const
    {
        return m_screenViewport;
    }
    D3D12_RECT GetScissorRect() const
    {
        return m_scissorRect;
    }
    UINT GetCurrentFrameIndex() const
    {
        return m_backBufferIndex;
    }
    UINT GetBackBufferCount() const
    {
        return m_backBufferCount;
    }
    DXGI_COLOR_SPACE_TYPE GetColorSpace() const
    {
        return m_colorSpace;
    }
    unsigned int GetDeviceOptions() const
    {
        return m_options;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetRenderTargetView() const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), m_backBufferIndex, m_rtvDescriptorSize);
    }
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetDepthStencilView() const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    }
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetFontHeapCPUView() const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_fntDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    }
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetFontHeapGPUView() const
    {
        return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_fntDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    }
    ID3D12DescriptorHeap* const* GetFontHeap() const
    {
        return m_fntDescriptorHeap.GetAddressOf();
    }

private:
    void MoveToNextFrame();
    void GetAdapter(IDXGIAdapter1** ppAdapter);
    void UpdateColorSpace();

    static const size_t MAX_BACK_BUFFER_COUNT = 3;

    UINT m_backBufferIndex;

    // Direct3D objects.
    Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocators[MAX_BACK_BUFFER_COUNT];

    // Swap chain objects.
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[MAX_BACK_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencil;

    // Presentation fence objects.
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValues[MAX_BACK_BUFFER_COUNT];
    Microsoft::WRL::Wrappers::Event m_fenceEvent;

    // Direct3D rendering objects.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvDescriptorHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_fntDescriptorHeap;
    UINT m_rtvDescriptorSize;
    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    // Direct3D properties.
    DXGI_FORMAT m_backBufferFormat;
    DXGI_FORMAT m_depthBufferFormat;
    UINT m_backBufferCount;
    D3D_FEATURE_LEVEL m_d3dMinFeatureLevel;

    // Cached device properties.
    HWND m_window;
    D3D_FEATURE_LEVEL m_d3dFeatureLevel;
    DWORD m_dxgiFactoryFlags;
    RECT m_outputSize;

    // HDR Support
    DXGI_COLOR_SPACE_TYPE m_colorSpace;

    // DxDeviceResources options (see flags above)
    unsigned int m_options;

    // The IDeviceNotify can be held directly as it owns the DxDeviceResources.
    Mgfx::IDeviceNotify* m_pDeviceNotify;
};

} // namespace Mgfx
