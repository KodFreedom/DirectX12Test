#include "render_system.h"
#include "main.h"

////////////////////////////////////////////////////////////////////////
//
//  Ã“Iƒƒ“ƒo[
//
////////////////////////////////////////////////////////////////////////
const DXGI_FORMAT RenderSystem::kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

////////////////////////////////////////////////////////////////////////
//
//  Public
//
////////////////////////////////////////////////////////////////////////
RenderSystem::RenderSystem()
{
}

RenderSystem::~RenderSystem()
{
    // The reason we need to flush the command queue in the
    // destructor is that we need to wait until the GPU is done processing the commands in
    // the queue before we destroy any resources the GPU is still referencing.Otherwise,
    // the GPU might crash when the application exits.
    if (device_ != nullptr)    {        FlushCommandQueue();    }
}

void RenderSystem::Init(HWND hwnd, UINT width, UINT height)
{
    hwnd_ = hwnd;
    width_ = width;
    height_ = height;
    aspect_ratio_ = (float)width_ / height_;

    // 0. Create the IDXGIFactory4
    CreateFactory();

    // 1. Create the ID3D12Device using the D3D12CreateDevice function.
    CreateDevice();

    // 2. Create an ID3D12Fence object and query descriptor sizes.
    CreateFenceAndDescriptorSizes();

    // 3. Check 4X MSAA quality level support.
    CheckMsaaSupport();

    // 4. Create the command queue, command list allocator, and main command list.
    CreateCommandQueueAndCommandList();

    // 5. Describe and create the swap chain.
    CreateSwapChain();

    // 6. Create the descriptor heaps the application requires.
    CreateDescriptorHeap();

    // 7. Resize the back buffer and create a render target view to the back buffer.
    CreateRenderTargetView();

    // 8. Create the depth/stencil buffer and its associated depth/stencil view.
    CreateDepthStencilView();

    // 9. Set the viewport and scissor rectangles.

    SetViewport();
    SetScissorRectangles();
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

void RenderSystem::PrepareRender()
{

}

void RenderSystem::Render()
{

}

void RenderSystem::SetMsaaEnable(bool value)
{
    if (msaa_enable_ != value)
    {
        msaa_enable_ = value;

        // Recreate the swapchain and buffers with new multisample settings.
        CreateSwapChain();
        OnResize();
    }
}

void RenderSystem::OnResize()
{
    assert(device_);
    assert(swap_chain_);
    assert(command_allocator_);

    // Flush before changing any resources.
    FlushCommandQueue();

    ThrowIfFailed(command_list_->Reset(command_allocator_.Get(), nullptr));

    // Release the previous resources we will be recreating.
    for (int iterator = 0; iterator < kSwapChainBufferCount; ++iterator)
    {
        render_targets_[iterator].Reset();
    }
    depth_stencil_view_.Reset();

    // Resize the swap chain.
    ThrowIfFailed(swap_chain_->ResizeBuffers(
        kSwapChainBufferCount,
        width_,
        height_,
        kBackBufferFormat,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    current_back_buffer_index_ = 0;

    CreateRenderTargetView();
    CreateDepthStencilView();

    // Execute the resize commands.
    ThrowIfFailed(command_list_->Close());
    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

    // Wait until resize is complete.
    FlushCommandQueue();

    // Update the viewport transform to cover the client area.
    CalculateViewportAndScissorRectangles();
}

////////////////////////////////////////////////////////////////////////
//
//  Private
//
////////////////////////////////////////////////////////////////////////
void RenderSystem::CreateFactory()
{
    UINT dxgi_factory_flags = 0;

#if defined(_DEBUG)
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // Enabling the debug layer after device creation will invalidate the active device.
    // Direct3D will enable extra debugging and send debug messages to the VC++ output window like the following :    // D3D12 ERROR: ID3D12CommandList::Reset: Reset fails because the command list was not closed.
    {
        ComPtr<ID3D12Debug> debug_controller;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)));
        debug_controller->EnableDebugLayer();

        // Enable additional debug layers.
        dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // used to create the IDXGISwapChain interface and enumerate display adapters.
    // In order to create a WARP adapter, we need to create an IDXGIFactory4 object so that we can enumerate
    // the warp adapter :
    ThrowIfFailed(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory_)));
}

void RenderSystem::CreateDevice()
{
    // Try to create hardware device.
    HRESULT hr = D3D12CreateDevice(
        nullptr, // default adapter
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_));

    // Fallback to WARP device.    // if our call to D3D12CreateDevice fails, we fallback to a WARP
    // device, which is a software adapter.WARP stands for Windows Advanced Rasterization
    // Platform.On Windows 7 and lower, the WARP device supports up to feature level 10.1;
    // on Windows 8, the WARP device supports up to feature level 11.1.I    if (FAILED(hr))
    {
        ComPtr<IDXGIAdapter> warp_adapter;
        ThrowIfFailed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));
        ThrowIfFailed(D3D12CreateDevice(
            warp_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device_)));
    }
}

void RenderSystem::CreateFenceAndDescriptorSizes()
{
    ThrowIfFailed(device_->CreateFence(
        0,
        D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence_)));

    // Descriptor sizes can vary across GPUs so we need to query this
    // information.We cache the descriptor sizes so that it is available when we need it for
    // various descriptor types :
    rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    cbv_srv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void RenderSystem::CheckMsaaSupport()
{
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality_levels;
    quality_levels.Format = kBackBufferFormat;
    quality_levels.SampleCount = msaa_sample_count_;
    quality_levels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    quality_levels.NumQualityLevels = 0;

    ThrowIfFailed(device_->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &quality_levels,
        sizeof(quality_levels)));

    msaa_quality_ = quality_levels.NumQualityLevels;

    // Because 4X MSAA is always supported, the returned quality should always be greater
    // than 0; therefore, we assert that this is the case.
    assert(msaa_quality_ > 0 && "Unexpected MSAA qualitylevel.");
}

void RenderSystem::CreateCommandQueueAndCommandList()
{
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(device_->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue_)));

    ThrowIfFailed(device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(command_allocator_.GetAddressOf())));    ThrowIfFailed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_allocator_.Get(), // Associated command allocator 
        nullptr, // Initial PipelineStateObject
        IID_PPV_ARGS(command_list_.GetAddressOf())));
    // Start off in a closed state. This is because the first time we
    // refer to the command list we will Reset it, and it needs to be
    // closed before calling Reset.
    command_list_->Close();
}

void RenderSystem::CreateSwapChain()
{
    // Observe that this function has been designed so that it can be called multiple times. It will
    // destroy the old swap chain before creating the new one.This allows us to recreate the
    // swap chain with different settings; in particular, we can change the multisampling settings
    // at runtime.

    // Release the previous swapchain we will be recreating.
    swap_chain_.Reset();

    DXGI_SWAP_CHAIN_DESC swap_chain_desc;
    swap_chain_desc.BufferDesc.Width = width_;
    swap_chain_desc.BufferDesc.Height = height_;
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.BufferDesc.Format = kBackBufferFormat;
    swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swap_chain_desc.SampleDesc.Count = msaa_enable_ ? 4 : 1;
    swap_chain_desc.SampleDesc.Quality = msaa_enable_ ? (msaa_quality_ - 1) : 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = kSwapChainBufferCount;
    swap_chain_desc.OutputWindow = hwnd_;
    swap_chain_desc.Windowed = true;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Note: Swap chain uses queue to perform flush.
    ThrowIfFailed(factory_->CreateSwapChain(
        command_queue_.Get(),
        &swap_chain_desc,
        swap_chain_.GetAddressOf()));
}

void RenderSystem::CreateDescriptorHeap()
{
    // we need SwapChainBufferCount many
    // render target views(RTVs) to describe the buffer resources in the swap chain we will
    // render into, and one depth / stencil view(DSV) to describe the depth / stencil buffer resource
    // for depth testing.Therefore, we need a heap for storing SwapChainBufferCount
    // RTVs, and we need a heap for storing one DSV.

    // render target view
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc;
    rtv_heap_desc.NumDescriptors = kSwapChainBufferCount;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtv_heap_desc.NodeMask = 0;

    ThrowIfFailed(device_->CreateDescriptorHeap(
        &rtv_heap_desc,
        IID_PPV_ARGS(rtv_heap_.GetAddressOf())));

    // depth/stencil view
    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc;
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsv_heap_desc.NodeMask = 0;

    ThrowIfFailed(device_->CreateDescriptorHeap(
        &dsv_heap_desc,
        IID_PPV_ARGS(dsv_heap_.GetAddressOf())));
}

void RenderSystem::CreateRenderTargetView()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_heap_handle(rtv_heap_->GetCPUDescriptorHandleForHeapStart());

    for (UINT iterator = 0; iterator < kSwapChainBufferCount; ++iterator)
    {
        // Get the ith buffer in the swap chain.
        ThrowIfFailed(swap_chain_->GetBuffer(
            iterator,
            IID_PPV_ARGS(&render_targets_[iterator])));

        // Create an RTV to it.
        device_->CreateRenderTargetView(
            render_targets_[iterator].Get(),
            nullptr,
            rtv_heap_handle);

        // Next entry in heap.
        rtv_heap_handle.Offset(1, rtv_descriptor_size_);
    }
}

void RenderSystem::CreateDepthStencilView()
{
    // The depth buffer is a texture, so it must be created with certain data formats. The
    // formats used for depth buffering are as follows:
    // 1. DXGI_FORMAT_D32_FLOAT_S8X24_UINT: Specifies a 32-bit floating-point
    //    depth buffer, with 8-bits (unsigned integer) reserved for the stencil buffer mapped to
    //    the [0, 255] range and 24-bits not used for padding.
    // 2. DXGI_FORMAT_D32_FLOAT: Specifies a 32-bit floating-point depth buffer.
    // 3. DXGI_FORMAT_D24_UNORM_S8_UINT: Specifies an unsigned 24-bit depth
    //    buffer mapped to the [0, 1] range with 8-bits (unsigned integer) reserved for the
    //    stencil buffer mapped to the [0, 255] range.
    // 4. DXGI_FORMAT_D16_UNORM: Specifies an unsigned 16-bit depth buffer mapped
    //    to the [0, 1] range.
    DXGI_FORMAT depth_stencil_format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

    // before using the depth / stencil buffer, we must create an associated
    // depth / stencil view to be bound to the pipeline.This is done similarly to creating the render
    // target view
    // Create the depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depth_stencil_desc;
    depth_stencil_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth_stencil_desc.Alignment = 0;
    depth_stencil_desc.Width = width_;
    depth_stencil_desc.Height = height_;
    depth_stencil_desc.DepthOrArraySize = 1;
    depth_stencil_desc.MipLevels = 1;

    // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
    // the depth buffer.  Therefore, because we need to create two views to the same resource:
    //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    // we need to create the depth buffer resource with a typeless format.  
    depth_stencil_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depth_stencil_desc.SampleDesc.Count = msaa_enable_ ? 4 : 1;
    depth_stencil_desc.SampleDesc.Quality = msaa_enable_ ? (msaa_quality_ - 1) : 0;
    depth_stencil_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depth_stencil_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE opt_clear;
    opt_clear.Format = depth_stencil_format;
    opt_clear.DepthStencil.Depth = 1.0f;
    opt_clear.DepthStencil.Stencil = 0;

    // Note that we use the CD3DX12_HEAP_PROPERTIES helper constructor to create
    // the heap properties structure,
    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depth_stencil_desc,
        D3D12_RESOURCE_STATE_COMMON,
        &opt_clear,
        IID_PPV_ARGS(depth_stencil_view_.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
    dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Format = depth_stencil_format;
    dsv_desc.Texture2D.MipSlice = 0;

    device_->CreateDepthStencilView(
        depth_stencil_view_.Get(),
        &dsv_desc,
        DepthStencilView());

    // Transition the resource from its initial state to
    // be used as a depth buffer.
    command_list_->ResourceBarrier(
        1,
        &CD3DX12_RESOURCE_BARRIER::Transition(
            depth_stencil_view_.Get(),
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_STATE_DEPTH_WRITE));
}

void RenderSystem::CalculateViewportAndScissorRectangles()
{
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<float>(width_);
    viewport_.Height = static_cast<float>(height_);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissor_rect_ = D3D12_RECT{ 0, 0, width_ / 2, height_ / 2 };
}

void RenderSystem::SetViewport()
{
    // The viewport needs to be reset whenever the command list is reset.
    command_list_->RSSetViewports(        1, // The first parameter is the number of viewports to bind (using more than one is for
           // advanced effects), and the second parameter is a pointer to an array of viewports.           // You cannot specify multiple viewports to the same render target. Multiple
           // viewports are used for advanced techniques that render to multiple render
           // targets at the same time.        &viewport_);
}

void RenderSystem::SetScissorRectangles()
{
    // The scissors rectangles need to be reset whenever the command list is reset
    command_list_->RSSetScissorRects(1, &scissor_rect_);
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderSystem::CurrentBackBufferView() const
{
    // CD3DX12 constructor to offset to the RTV of the current back buffer.
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            rtv_heap_->GetCPUDescriptorHandleForHeapStart(),// handle start
            current_back_buffer_index_, // index to offset
            rtv_descriptor_size_);      // byte size of descriptor
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderSystem::DepthStencilView() const
{
    return dsv_heap_ ->GetCPUDescriptorHandleForHeapStart();
}

void RenderSystem::LoadAssets()
{
    //// Create the command list.
    //ThrowIfFailed(device_->CreateCommandList(
    //    0,
    //    D3D12_COMMAND_LIST_TYPE_DIRECT,
    //    command_allocator_.Get(),
    //    nullptr,
    //    IID_PPV_ARGS(&command_list_)));
    //
    //// Command lists are created in the recording state, but there is nothing
    //// to record yet. The main loop expects it to be closed, so close it now.
    //ThrowIfFailed(command_list_->Close());

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