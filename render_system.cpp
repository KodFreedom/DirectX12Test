#include "render_system.h"
#include "d3dx12.h"

RenderSystem::RenderSystem()
{
}

RenderSystem::~RenderSystem()
{

}

void RenderSystem::Init(HWND hwnd, UINT width, UINT height)
{
    width_ = width;
    height_ = height;
    aspect_ratio_ = (float)width_ / height_;
    LoadPipeline(hwnd);
    LoadAssets();
}

void RenderSystem::Uninit()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(fence_event_);
}

void RenderSystem::Update()
{

}

void RenderSystem::Draw()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(swap_chain_->Present(1, 0));

    WaitForPreviousFrame();
}

void RenderSystem::LoadPipeline(HWND hwnd)
{
    UINT dxgi_factory_flags = 0;
    HRESULT hr;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the active device.
    {
        ComPtr<ID3D12Debug> debug_controller;
        hr = D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller));
        if (SUCCEEDED(hr))
        {
            debug_controller->EnableDebugLayer();

            // Enable additional debug layers.
            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory)));

    if (use_warp_device_)
    {
        ComPtr<IDXGIAdapter> warp_adapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warp_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device_)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardware_adapter;
        GetHardwareAdapter(factory.Get(), &hardware_adapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardware_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device_)));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue_)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.BufferCount = kFrameCount;
    swap_chain_desc.Width = width_;
    swap_chain_desc.Height = height_;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swap_chain = nullptr;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        command_queue_.Get(),		// Swap chain needs the queue so that it can force a flush on it.
        hwnd,
        &swap_chain_desc,
        nullptr,
        nullptr,
        &swap_chain));

    // This sample does not support fullscreen transitions.
    ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
    ThrowIfFailed(swap_chain.As(&swap_chain_));
    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc = {};
        rtv_heap_desc.NumDescriptors = kFrameCount;
        rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(device_->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&rtv_heap_)));

        rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(rtv_heap_->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < kFrameCount; n++)
        {
            hr = swap_chain_->GetBuffer(n, IID_PPV_ARGS(&render_targets_[n]));
            device_->CreateRenderTargetView(render_targets_[n].Get(), nullptr, rtv_handle);
            rtv_handle.Offset(1, rtv_descriptor_size_);
        }
    }

    ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&command_allocator_)));
}

void RenderSystem::LoadAssets()
{
    // Create the command list.
    ThrowIfFailed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator_.Get(),
        nullptr,
        IID_PPV_ARGS(&command_list_)));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    ThrowIfFailed(command_list_->Close());

    // Create synchronization objects.
    {
        ThrowIfFailed(device_->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&fence_)));

        fence_value_ = 1;

        // Create an event handle to use for frame synchronization.
        fence_event_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (fence_event_ == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }
    }
}

void RenderSystem::ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr))
    {
        int error = 0;
    }
}

void RenderSystem::GetHardwareAdapter(_In_ IDXGIFactory2* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter)
{
    ComPtr<IDXGIAdapter1> adapter;
    *ppAdapter = nullptr;

    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            // If you want a software adapter, pass in "/warp" on the command line.
            continue;
        }

        // Check to see if the adapter supports Direct3D 12, but don't create the
        // actual device yet.
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
        {
            break;
        }
    }

    *ppAdapter = adapter.Detach();
}

void RenderSystem::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(command_allocator_->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(command_list_->Reset(command_allocator_.Get(), pipeline_state_.Get()));

    // Indicate that the back buffer will be used as a render target.
    command_list_->ResourceBarrier(
        1, 
        &CD3DX12_RESOURCE_BARRIER::Transition(
            render_targets_[frame_index_].Get(),
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_handle(
        rtv_heap_->GetCPUDescriptorHandleForHeapStart(),
        frame_index_, 
        rtv_descriptor_size_);

    // Record commands.
    const float clear_color[] = { 1.0f, 0.2f, 0.4f, 1.0f };
    command_list_->ClearRenderTargetView(
        rtv_handle, 
        clear_color, 
        0, 
        nullptr);

    // Indicate that the back buffer will now be used to present.
    command_list_->ResourceBarrier(
        1, 
        &CD3DX12_RESOURCE_BARRIER::Transition(
            render_targets_[frame_index_].Get(), 
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(command_list_->Close());
}

void RenderSystem::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = fence_value_;
    ThrowIfFailed(command_queue_->Signal(fence_.Get(), fence));
    fence_value_++;

    // Wait until the previous frame is finished.
    if (fence_->GetCompletedValue() < fence)
    {
        ThrowIfFailed(fence_->SetEventOnCompletion(fence, fence_event_));
        WaitForSingleObject(fence_event_, INFINITE);
    }

    frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
}