#include "render_system.h"
#include "game_system.h"
#include <DirectXColors.h>

using namespace DirectX;
using Microsoft::WRL::ComPtr;
using namespace std;

//--------------------------------------------------------------------------------
//
//  Public
//
//--------------------------------------------------------------------------------
RenderSystem* RenderSystem::Create()
{
    RenderSystem* render_system = new RenderSystem;
    return render_system;
}

bool RenderSystem::Initialize()
{
#if defined(DEBUG) || defined(_DEBUG) 
    // Enable the D3D12 debug layer.
    {
        ComPtr<ID3D12Debug> debug_controller;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_controller)));
        debug_controller->EnableDebugLayer();
    }
#endif

    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory_)));

    // Try to create hardware device.
    HRESULT hardware_result = D3D12CreateDevice(
        nullptr,             // default adapter
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&device_));

    // Fallback to WARP device.
    if (FAILED(hardware_result))
    {
        ComPtr<IDXGIAdapter> warp_adapter;
        ThrowIfFailed(factory_->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warp_adapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device_)));
    }

    ThrowIfFailed(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE,
        IID_PPV_ARGS(&fence_)));

    rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    dsv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    cbv_srv_uav_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Check 4X MSAA quality support for our back buffer format.
    // All Direct3D 11 capable devices support 4X MSAA for all render 
    // target formats, so we only need to check quality support.

    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS quality_levels;
    quality_levels.Format = back_buffer_format_;
    quality_levels.SampleCount = 4;
    quality_levels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    quality_levels.NumQualityLevels = 0;
    ThrowIfFailed(device_->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &quality_levels,
        sizeof(quality_levels)));

    msaa_quality_ = quality_levels.NumQualityLevels;
    assert(msaa_quality_ > 0 && "Unexpected MSAA quality level.");

#ifdef _DEBUG
    LogAdapters();
#endif

    CreateCommandObjects();
    CreateSwapChain();
    CreateRtvAndDsvDescriptorHeaps();

    OnResize();
    return true;
}

void RenderSystem::Release()
{
    delete this;
}

void RenderSystem::PrepareRender()
{

}

void RenderSystem::Render()
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(command_list_allocator_->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(command_list_->Reset(command_list_allocator_.Get(), nullptr));

    // Indicate a state transition on the resource usage.
    command_list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
    command_list_->RSSetViewports(1, &screen_viewport_);
    command_list_->RSSetScissorRects(1, &scissor_rect_);

    // Clear the back buffer and depth buffer.
    command_list_->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    command_list_->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    command_list_->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // Indicate a state transition on the resource usage.
    command_list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(command_list_->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

    // swap the back and front buffers
    ThrowIfFailed(swap_chain_->Present(0, 0));
    current_back_buffer_ = (current_back_buffer_ + 1) % kSwapChainBufferCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void RenderSystem::OnResize()
{
    assert(device_);
    assert(swap_chain_);
    assert(command_list_allocator_);

    // Flush before changing any resources.
    FlushCommandQueue();

    ThrowIfFailed(command_list_->Reset(command_list_allocator_.Get(), nullptr));

    // Release the previous resources we will be recreating.
    for (int i = 0; i < kSwapChainBufferCount; ++i)
    {
        swap_chain_buffer_[i].Reset();
    }

    depth_stencil_buffer_.Reset();

    // Resize the swap chain.
    ThrowIfFailed(swap_chain_->ResizeBuffers(
        kSwapChainBufferCount,
        GameSystem::Instance().Width(),
        GameSystem::Instance().Height(),
        back_buffer_format_,
        DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    current_back_buffer_ = 0;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv_heap_handle(rtv_heap_->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < kSwapChainBufferCount; i++)
    {
        ThrowIfFailed(swap_chain_->GetBuffer(i, IID_PPV_ARGS(&swap_chain_buffer_[i])));
        device_->CreateRenderTargetView(swap_chain_buffer_[i].Get(), nullptr, rtv_heap_handle);
        rtv_heap_handle.Offset(1, rtv_descriptor_size_);
    }

    // Create the depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depth_stencil_desc;
    depth_stencil_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth_stencil_desc.Alignment = 0;
    depth_stencil_desc.Width = GameSystem::Instance().Width();
    depth_stencil_desc.Height = GameSystem::Instance().Height();
    depth_stencil_desc.DepthOrArraySize = 1;
    depth_stencil_desc.MipLevels = 1;

    // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
    // the depth buffer.  Therefore, because we need to create two views to the same resource:
    //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    // we need to create the depth buffer resource with a typeless format.  
    depth_stencil_desc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depth_stencil_desc.SampleDesc.Count = msaa_state_ ? 4 : 1;
    depth_stencil_desc.SampleDesc.Quality = msaa_state_ ? (msaa_quality_ - 1) : 0;
    depth_stencil_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depth_stencil_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE opt_clear;
    opt_clear.Format = depth_stencil_format_;
    opt_clear.DepthStencil.Depth = 1.0f;
    opt_clear.DepthStencil.Stencil = 0;
    ThrowIfFailed(device_->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &depth_stencil_desc,
        D3D12_RESOURCE_STATE_COMMON,
        &opt_clear,
        IID_PPV_ARGS(depth_stencil_buffer_.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc;
    dsv_desc.Flags = D3D12_DSV_FLAG_NONE;
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Format = depth_stencil_format_;
    dsv_desc.Texture2D.MipSlice = 0;
    device_->CreateDepthStencilView(depth_stencil_buffer_.Get(), &dsv_desc, DepthStencilView());

    // Transition the resource from its initial state to be used as a depth buffer.
    command_list_->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depth_stencil_buffer_.Get(),
        D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Execute the resize commands.
    ThrowIfFailed(command_list_->Close());
    ID3D12CommandList* command_lists[] = { command_list_.Get() };
    command_queue_->ExecuteCommandLists(_countof(command_lists), command_lists);

    // Wait until resize is complete.
    FlushCommandQueue();

    // Update the viewport transform to cover the client area.
    screen_viewport_.TopLeftX = 0;
    screen_viewport_.TopLeftY = 0;
    screen_viewport_.Width = static_cast<float>(GameSystem::Instance().Width());
    screen_viewport_.Height = static_cast<float>(GameSystem::Instance().Height());
    screen_viewport_.MinDepth = 0.0f;
    screen_viewport_.MaxDepth = 1.0f;

    scissor_rect_ = { 0, 0, (LONG)GameSystem::Instance().Width(), (LONG)GameSystem::Instance().Height() };
}

bool RenderSystem::GetMsaaState() const
{
    return msaa_state_;
}

void RenderSystem::SetMsaaState(bool value)
{
    msaa_state_ = value;
}

// Convenience overrides for handling mouse input.
void RenderSystem::OnMouseDown(WPARAM state, int x, int y)
{

}

void RenderSystem::OnMouseUp(WPARAM state, int x, int y)
{

}

void RenderSystem::OnMouseMove(WPARAM state, int x, int y)
{

}

//--------------------------------------------------------------------------------
//
//  Private
//
//--------------------------------------------------------------------------------
RenderSystem::RenderSystem()
{

}

RenderSystem::~RenderSystem()
{

}

void RenderSystem::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc;
    rtv_heap_desc.NumDescriptors = kSwapChainBufferCount;
    rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtv_heap_desc.NodeMask = 0;
    ThrowIfFailed(device_->CreateDescriptorHeap(
        &rtv_heap_desc, IID_PPV_ARGS(rtv_heap_.GetAddressOf())));


    D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc;
    dsv_heap_desc.NumDescriptors = 1;
    dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsv_heap_desc.NodeMask = 0;
    ThrowIfFailed(device_->CreateDescriptorHeap(
        &dsv_heap_desc, IID_PPV_ARGS(dsv_heap_.GetAddressOf())));
}

void RenderSystem::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ThrowIfFailed(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&command_queue_)));

    ThrowIfFailed(device_->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(command_list_allocator_.GetAddressOf())));

    ThrowIfFailed(device_->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        command_list_allocator_.Get(), // Associated command allocator
        nullptr,                   // Initial PipelineStateObject
        IID_PPV_ARGS(command_list_.GetAddressOf())));

    // Start off in a closed state.  This is because the first time we refer 
    // to the command list we will Reset it, and it needs to be closed before
    // calling Reset.
    command_list_->Close();
}

void RenderSystem::CreateSwapChain()
{
    // Release the previous swapchain we will be recreating.
    swap_chain_.Reset();

    DXGI_SWAP_CHAIN_DESC swap_chain_desc;
    swap_chain_desc.BufferDesc.Width = GameSystem::Instance().Width();
    swap_chain_desc.BufferDesc.Height = GameSystem::Instance().Height();
    swap_chain_desc.BufferDesc.RefreshRate.Numerator = 60;
    swap_chain_desc.BufferDesc.RefreshRate.Denominator = 1;
    swap_chain_desc.BufferDesc.Format = back_buffer_format_;
    swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swap_chain_desc.SampleDesc.Count = msaa_state_ ? 4 : 1;
    swap_chain_desc.SampleDesc.Quality = msaa_state_ ? (msaa_quality_ - 1) : 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = kSwapChainBufferCount;
    swap_chain_desc.OutputWindow = GameSystem::Instance().MainWindowHandle();
    swap_chain_desc.Windowed = true;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Note: Swap chain uses queue to perform flush.
    ThrowIfFailed(factory_->CreateSwapChain(
        command_queue_.Get(),
        &swap_chain_desc,
        swap_chain_.GetAddressOf()));
}

void RenderSystem::FlushCommandQueue()
{
    // Advance the fence value to mark commands up to this fence point.
    current_fence_++;

    // Add an instruction to the command queue to set a new fence point.  Because we 
    // are on the GPU timeline, the new fence point won't be set until the GPU finishes
    // processing all the commands prior to this Signal().
    ThrowIfFailed(command_queue_->Signal(fence_.Get(), current_fence_));

    // Wait until the GPU has completed commands up to this fence point.
    if (fence_->GetCompletedValue() < current_fence_)
    {
        HANDLE event_handle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence.  
        ThrowIfFailed(fence_->SetEventOnCompletion(current_fence_, event_handle));

        // Wait until the GPU hits current fence event is fired.
        WaitForSingleObject(event_handle, INFINITE);
        CloseHandle(event_handle);
    }
}

ID3D12Resource* RenderSystem::CurrentBackBuffer() const
{
    return swap_chain_buffer_[current_back_buffer_].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderSystem::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        rtv_heap_->GetCPUDescriptorHandleForHeapStart(),
        current_back_buffer_,
        rtv_descriptor_size_);
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderSystem::DepthStencilView() const
{
    return dsv_heap_->GetCPUDescriptorHandleForHeapStart();
}

void RenderSystem::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* adapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;
    while (factory_->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        std::wstring text = L"***Adapter: ";
        text += desc.Description;
        text += L"\n";

        OutputDebugString(text.c_str());

        adapterList.push_back(adapter);

        ++i;
    }

    for (size_t i = 0; i < adapterList.size(); ++i)
    {
        LogAdapterOutputs(adapterList[i]);
        ReleaseCom(adapterList[i]);
    }
}

void RenderSystem::LogAdapterOutputs(IDXGIAdapter* adapter)
{
    UINT i = 0;
    IDXGIOutput* output = nullptr;
    while (adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);

        std::wstring text = L"***Output: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugString(text.c_str());

        LogOutputDisplayModes(output, back_buffer_format_);

        ReleaseCom(output);

        ++i;
    }
}

void RenderSystem::LogOutputDisplayModes(IDXGIOutput* output, DXGI_FORMAT format)
{
    UINT count = 0;
    UINT flags = 0;

    // Call with nullptr to get list count.
    output->GetDisplayModeList(format, flags, &count, nullptr);

    std::vector<DXGI_MODE_DESC> modeList(count);
    output->GetDisplayModeList(format, flags, &count, &modeList[0]);

    for (auto& x : modeList)
    {
        UINT n = x.RefreshRate.Numerator;
        UINT d = x.RefreshRate.Denominator;
        std::wstring text =
            L"Width = " + std::to_wstring(x.Width) + L" " +
            L"Height = " + std::to_wstring(x.Height) + L" " +
            L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
            L"\n";

        ::OutputDebugString(text.c_str());
    }
}