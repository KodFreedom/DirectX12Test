#pragma once

#include "d3dUtil.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class RenderSystem
{
public:
    static RenderSystem* Create();
    
    bool Initialize();
    void Release();

    void PrepareRender();
    void Render();

    void OnResize();

    bool GetMsaaState()const;
    void SetMsaaState(bool value);

    // Convenience overrides for handling mouse input.
    void OnMouseDown(WPARAM state, int x, int y);
    void OnMouseUp(WPARAM state, int x, int y);
    void OnMouseMove(WPARAM state, int x, int y);

private:
    RenderSystem();
    RenderSystem(const RenderSystem& rhs) = delete;
    RenderSystem& operator=(const RenderSystem& rhs) = delete;
    ~RenderSystem();

    void CreateRtvAndDsvDescriptorHeaps();
    void CreateCommandObjects();
    void CreateSwapChain();

    void FlushCommandQueue();

    ID3D12Resource* CurrentBackBuffer()const;
    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

    void LogAdapters();
    void LogAdapterOutputs(IDXGIAdapter* adapter);
    void LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format);

    static constexpr int kSwapChainBufferCount = 2;

    // Set true to use 4X MSAA (§4.1.8).  The default is false.
    bool msaa_state_ = false;    // 4X MSAA enabled
    UINT msaa_quality_ = 0;      // quality level of 4X MSAA

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swap_chain_;
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    UINT64 current_fence_ = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> command_queue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> command_list_allocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> command_list_;

    int current_back_buffer_ = 0;
    Microsoft::WRL::ComPtr<ID3D12Resource> swap_chain_buffer_[kSwapChainBufferCount];
    Microsoft::WRL::ComPtr<ID3D12Resource> depth_stencil_buffer_;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsv_heap_;

    D3D12_VIEWPORT screen_viewport_;
    D3D12_RECT scissor_rect_;

    UINT rtv_descriptor_size_ = 0;
    UINT dsv_descriptor_size_ = 0;
    UINT cbv_srv_uav_descriptor_size_ = 0;

    D3D_DRIVER_TYPE driver_type_ = D3D_DRIVER_TYPE_HARDWARE;
    DXGI_FORMAT back_buffer_format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT depth_stencil_format_ = DXGI_FORMAT_D24_UNORM_S8_UINT;
};