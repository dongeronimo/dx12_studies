#include "pch.h"
#include "offscreen_render_target.h"
#include "direct3d_context.h"
#include "d3dx12.h"
#include "shared_descriptor_heap_v2.h"
//A global descriptor heap to make management easier for now
ID3D12DescriptorHeap* rtvHeap = nullptr;
ID3D12DescriptorHeap* dsvHeap = nullptr;
UINT rtvDescSize = 0;
UINT dsvDescSize = 0;
UINT rtvDescIdx = 0;
UINT dsvDescIdx = 0;

D3D12_RESOURCE_DESC CreateDesc(int w, int h, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags) {
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = w;
    desc.Height = h;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    return desc;
}

transforms::OffscreenRenderTarget::OffscreenRenderTarget(int w, int h, 
    transforms::Context* ctx, transforms::SharedDescriptorHeapV2* descriptorHeap)
{
    //if i haven't created the global descriptor heaps then create them, get their sizes, etc...
    if (rtvHeap == nullptr) {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = 1000;  // 1000 render targets
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ctx->GetDevice()->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap));
        rtvHeap->SetName(L"OffscreenRenderTargetRTVHeap");
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1000;  // 1 depth buffer
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ctx->GetDevice()->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
        rtvDescSize = ctx->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        dsvDescSize = ctx->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }
    srvCPUHandle.resize(FRAMEBUFFER_COUNT);
    srvGPUHandle.resize(FRAMEBUFFER_COUNT);
    renderTargetTexture.resize(FRAMEBUFFER_COUNT);
    depthTexture.resize(FRAMEBUFFER_COUNT);
    rtvHandle.resize(FRAMEBUFFER_COUNT);
    dsvHandle.resize(FRAMEBUFFER_COUNT);
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;
    for (int i = 0; i < FRAMEBUFFER_COUNT; i++) {
        //create the color texture
        D3D12_RESOURCE_DESC descForColorTexture = CreateDesc(w, h, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        D3D12_CLEAR_VALUE clearValueForColorTexture = {};
        clearValueForColorTexture.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        clearValueForColorTexture.Color[0] = 0.0f;
        clearValueForColorTexture.Color[1] = 0.0f;
        clearValueForColorTexture.Color[2] = 0.0f;
        clearValueForColorTexture.Color[3] = 1.0f;

        HRESULT hr = ctx->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &descForColorTexture,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValueForColorTexture,
            IID_PPV_ARGS(&renderTargetTexture[i]));
        //create the depth texture
        D3D12_RESOURCE_DESC depthDesc = CreateDesc(w, h, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        D3D12_CLEAR_VALUE depthClearValue = {};
        depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthClearValue.DepthStencil.Depth = 1.0f;
        depthClearValue.DepthStencil.Stencil = 0;
        hr = ctx->GetDevice()->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthClearValue,
            IID_PPV_ARGS(&depthTexture[i]));
        //create the RTV descriptr
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        rtvDesc.Texture2D.PlaneSlice = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE _rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        _rtvHandle.ptr += static_cast<SIZE_T>(rtvDescIdx) * rtvDescSize;
        ctx->GetDevice()->CreateRenderTargetView(renderTargetTexture[i], &rtvDesc, _rtvHandle);
        rtvHandle[i] = _rtvHandle;
        rtvDescIdx++;
        //create the DSV descriptor
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        // Get the handle to the start of the heap.
        D3D12_CPU_DESCRIPTOR_HANDLE _dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        // Offset the handle by the correct number of descriptors.
        _dsvHandle.ptr += static_cast<SIZE_T>(dsvDescIdx) * dsvDescSize;
        ctx->GetDevice()->CreateDepthStencilView(depthTexture[i], &dsvDesc, _dsvHandle);
        dsvHandle[i] = _dsvHandle;
        dsvDescIdx++;

        auto [colorCPUSrvDescriptor, colorGPUSrvDescriptor] = descriptorHeap->AllocateDescriptor();
        srvCPUHandle[i] = colorCPUSrvDescriptor;
        srvGPUHandle[i] = colorGPUSrvDescriptor;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        ctx->GetDevice()->CreateShaderResourceView(renderTargetTexture[i], &srvDesc, srvCPUHandle[i]);
 
        std::wstringstream ssname;
        ssname << "OffsceeenColor" << i;
        std::wstring strname = ssname.str();
        renderTargetTexture[i]->SetName(strname.c_str());
    }
}

void transforms::OffscreenRenderTarget::SetAsRenderTarget(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
    commandList->OMSetRenderTargets(
        1,                                // one render target
        &rtvHandle[frameIndex],           // from your OffscreenRenderTarget instance
        FALSE,                            // no RTVs in a descriptor array
        &dsvHandle[frameIndex]);          // depth stencil
}

void transforms::OffscreenRenderTarget::Clear(ID3D12GraphicsCommandList* commandList, int frameIndex, std::array<float, 4> clearColor)
{
    commandList->ClearRenderTargetView(rtvHandle[frameIndex], clearColor.data(), 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle[frameIndex], D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void transforms::OffscreenRenderTarget::TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
    CD3DX12_RESOURCE_BARRIER barrierBack = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargetTexture[frameIndex],
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    commandList->ResourceBarrier(1, &barrierBack);
}

void transforms::OffscreenRenderTarget::TransitionToPixelShaderResource(ID3D12GraphicsCommandList* commandList, int frameIndex)
{
    CD3DX12_RESOURCE_BARRIER barrierBack = CD3DX12_RESOURCE_BARRIER::Transition(
        renderTargetTexture[frameIndex],
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrierBack);
}
