#pragma once

/////////////
// LINKING //
/////////////
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")


//////////////
// INCLUDES //
//////////////
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl\client.h>
using namespace Microsoft::WRL;
#include <DirectXMath.h>
#include "d3dx12.h"

class RenderSystem
{
public:
    RenderSystem();
    ~RenderSystem();

    void Init(HWND hwnd, UINT width, UINT height);
    void Uninit();
    void Update();
    void PrepareRender();
    void Render();
    ComPtr<ID3D12Device>& GetDevice()
    {
        return device_;
    }
    void SetMsaaEnable(bool value);
    void OnResize();

private:
    void CreateFactory();
    void CreateDevice();
    void CreateFenceAndDescriptorSizes();
    void CheckMsaaSupport();
    void CreateCommandQueueAndCommandList();
    void CreateSwapChain();
    void CreateDescriptorHeap();
    void CreateRenderTargetView();
    void CreateDepthStencilView();
    void CalculateViewportAndScissorRectangles();
    void SetViewport();
    void SetScissorRectangles();
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView() const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView() const;
    void FlushCommandQueue();
    void LoadAssets();
    void GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter);
    void WaitForPreviousFrame();

    // íËêî
    static const UINT kSwapChainBufferCount = 2;
    static const DXGI_FORMAT kBackBufferFormat;

    // Pipeline objects.
    ComPtr<IDXGIFactory4> factory_ = nullptr;
    ComPtr<ID3D12Device> device_ = nullptr;
    ComPtr<ID3D12CommandQueue> command_queue_ = nullptr;
    ComPtr<ID3D12CommandAllocator> command_allocator_ = nullptr;
    ComPtr<ID3D12GraphicsCommandList> command_list_ = nullptr;
    ComPtr<ID3D12Resource> render_targets_[kSwapChainBufferCount] = {};
    ComPtr<ID3D12Resource> depth_stencil_view_ = nullptr;
    ComPtr<ID3D12PipelineState> pipeline_state_ = nullptr;
    ComPtr<ID3D12RootSignature> root_signature_ = nullptr;

    // Swap chain
    ComPtr<IDXGISwapChain> swap_chain_ = nullptr;    // to change the front buffer and back buffer
    int current_back_buffer_index_ = 0;

    // Msaa
    bool msaa_enable_ = true;
    UINT msaa_sample_count_ = 4;
    UINT msaa_quality_ = 0;

    // Synchronization objects.
    UINT frame_index_;
    HANDLE fence_event_;
    ComPtr<ID3D12Fence> fence_;
    UINT64 fence_value_;

    // Descriptor Heap
    ComPtr<ID3D12DescriptorHeap> rtv_heap_ = nullptr; // render target view heap
    ComPtr<ID3D12DescriptorHeap> dsv_heap_ = nullptr; // depth/stencil view heap
    UINT rtv_descriptor_size_ = 0;
    UINT dsv_descriptor_size_ = 0;
    UINT cbv_srv_descriptor_size_ = 0;

    // Viewport dimensions.
    UINT width_ = 0;
    UINT height_ = 0;
    float aspect_ratio_ = 0.0f;
    D3D12_VIEWPORT viewport_ = {};
    D3D12_RECT scissor_rect_ = {};
    HWND hwnd_ = nullptr;
};