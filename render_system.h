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

class RenderSystem
{
public:
    RenderSystem();
    ~RenderSystem();

    void Init(HWND hwnd, UINT width, UINT height);
    void Uninit();
    void Update();
    void Draw();

private:
    void LoadPipeline(HWND hwnd);
    void LoadAssets();
    void GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter);
    void ThrowIfFailed(HRESULT hr);
    void PopulateCommandList();
    void WaitForPreviousFrame();

    static const UINT kFrameCount = 2;

    // Pipeline objects.
    ComPtr<IDXGISwapChain3> swap_chain_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12Resource> render_targets_[2];
    ComPtr<ID3D12CommandAllocator> command_allocator_;
    ComPtr<ID3D12CommandQueue> command_queue_;
    ComPtr<ID3D12DescriptorHeap> rtv_heap_;
    ComPtr<ID3D12PipelineState> pipeline_state_;
    ComPtr<ID3D12GraphicsCommandList> command_list_;
    UINT rtv_descriptor_size_;

    // Synchronization objects.
    UINT frame_index_;
    HANDLE fence_event_;
    ComPtr<ID3D12Fence> fence_;
    UINT64 fence_value_;

    // Adapter info.
    bool use_warp_device_;

    // Viewport dimensions.
    UINT width_;
    UINT height_;
    float aspect_ratio_;
};