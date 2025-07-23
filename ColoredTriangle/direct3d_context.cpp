#include "direct3d_context.h"
#include "../Common/d3d_utils.h"
#include "pipeline.h"
using Microsoft::WRL::ComPtr;
namespace common
{

    void Context::CreateVertexBufferFromData(std::vector<Vertex>& _vertices, Microsoft::WRL::ComPtr<ID3D12Resource>& _vertexBuffer, D3D12_VERTEX_BUFFER_VIEW& _vertexBufferView, const std::wstring& name)
    {
        assert(_vertexBuffer == nullptr);
        //size IN BYTES of the buffer
        int vBufferSize = _vertices.size() * sizeof(Vertex);
        CD3DX12_HEAP_PROPERTIES stagingHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(vBufferSize);
        device->CreateCommittedResource(
            &stagingHeapProperties, // a default heap
            D3D12_HEAP_FLAG_NONE, // no flags
            &resourceDesc, // resource description for a buffer
            D3D12_RESOURCE_STATE_COMMON,//the initial state of vertex buffer is D3D12_RESOURCE_STATE_COMMON. Before we use them i'll have to transition it to the correct state
            nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
            IID_PPV_ARGS(&_vertexBuffer));
        // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
        _vertexBuffer->SetName(name.c_str());
        // create upload heap
        // upload heaps are used to upload data to the GPU. CPU can write to it, GPU can read from it
        // We will upload the vertex buffer using this heap to the default heap
        ComPtr<ID3D12Resource> vBufferUploadHeap;
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        device->CreateCommittedResource(
            &uploadHeapProperties, // upload heap
            D3D12_HEAP_FLAG_NONE, // no flags
            &resourceDesc, // resource description for a buffer
            D3D12_RESOURCE_STATE_COMMON, //D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
            nullptr,
            IID_PPV_ARGS(&vBufferUploadHeap));
        vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");
        // store vertex buffer in upload heap
        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = reinterpret_cast<BYTE*>(_vertices.data()); // pointer to our vertex array
        vertexData.RowPitch = vBufferSize; // size of all our triangle vertex data
        vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data
        //move _vertexBuffer and vBufferUploadHeap from D3D12_RESOURCE_STATE_COMMON to their states, then copy the content
        //to _vertexBuffer
        common::RunCommands(
            device.Get(),
            commandQueue.Get(),
            [&_vertexBuffer, &vBufferUploadHeap, &vertexData](ComPtr<ID3D12GraphicsCommandList> lst) {
                //vertexBuffer will go from common to copy destination
                CD3DX12_RESOURCE_BARRIER vertexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    _vertexBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
                lst->ResourceBarrier(1, &vertexBufferResourceBarrier);
                //staging buffer will go from common to read origin
                CD3DX12_RESOURCE_BARRIER stagingBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    vBufferUploadHeap.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_GENERIC_READ);
                lst->ResourceBarrier(1, &stagingBufferResourceBarrier);
                //copy the vertex data from RAM to the vertex buffer, thru vBufferUploadHeap
                UpdateSubresources(lst.Get(), _vertexBuffer.Get(), vBufferUploadHeap.Get(), 0, 0, 1, &vertexData);
                //now that the data is in _vertexBuffer i transition _vertexBuffer to D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, so that
                //it can be used as vertex buffer
                CD3DX12_RESOURCE_BARRIER secondVertexBufferResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(_vertexBuffer.Get(),
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
                lst->ResourceBarrier(1, &secondVertexBufferResourceBarrier);
            });
        _vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
        _vertexBufferView.StrideInBytes = sizeof(Vertex);
        _vertexBufferView.SizeInBytes = vBufferSize;
    }
    void Context::WaitForPreviousFrame()
    {
        HRESULT hr;
        // if the current fence value is still less than "fenceValue", then we know the GPU has not finished executing
        // the command queue since it has not reached the "commandQueue->Signal(fence, fenceValue)" command
        if (fence[frameIndex]->GetCompletedValue() < fenceValue[frameIndex])
        {
            // a handle to an event when our fence is unlocked by the gpu
            HANDLE fenceEvent;
            fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            assert(fenceEvent != nullptr);
            // we have the fence create an event which is signaled once the fence's current value is "fenceValue"
            hr = fence[frameIndex]->SetEventOnCompletion(fenceValue[frameIndex], fenceEvent);
            assert(hr == S_OK);
            // We will wait until the fence has triggered the event that it's current value has reached "fenceValue". once it's value
            // has reached "fenceValue", we know the command queue has finished executing
            WaitForSingleObject(fenceEvent, INFINITE);
        }
        // increment fenceValue for next frame
        fenceValue[frameIndex]++;
        // swap the current rtv buffer index so we draw on the correct buffer
        frameIndex = swapChain->GetCurrentBackBufferIndex();
    }
    void Context::ResetCurrentCommandList()
    {
        //resetting an allocator frees the memory that the command list was stored in
        HRESULT hr = commandAllocator[frameIndex]->Reset();
        assert(hr == S_OK);
        //when we reset the command list we put it back to the recording state
        hr = commandList->Reset(commandAllocator[frameIndex].Get(), NULL);
        assert(hr == S_OK);
    }
    void Context::TransitionCurrentRenderTarget(D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
    {
        CD3DX12_RESOURCE_BARRIER fromPresentToRenderTarget = CD3DX12_RESOURCE_BARRIER::Transition(swapChainRenderTargets[frameIndex].Get(),
            from, ///Present is the state that the render target view has to be to be able to present the image
            to//Render target is the state that the RTV has to be to be used by the output merger state.
        );
        commandList->ResourceBarrier(1,
            &fromPresentToRenderTarget);
    }
    void Context::SetCurrentOutputMergerTarget()
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE _rtvHandle(
            rtvData->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            frameIndex, rtvData->rtvDescriptorSize);
        // set the render target for the output merger stage (the output of the pipeline)
        commandList->OMSetRenderTargets(1, &_rtvHandle, FALSE, nullptr);
        this->rtvHandle = _rtvHandle;
    }
    void Context::ClearRenderTargetView(std::array<float, 4> rgba)
    {
        commandList->ClearRenderTargetView(rtvHandle, rgba.data(), 0, nullptr);
    }
    void Context::BindRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature> rs)
    {
        commandList->SetGraphicsRootSignature(rs.Get());
    }
    void Context::BindPipeline(std::shared_ptr<Pipeline> pipe)
    {
        pipe->Bind(commandList);

    }
    void Context::Present()
    {
        HRESULT hr = commandList->Close();
        assert(hr == S_OK);
        std::array<ID3D12CommandList*, 1> ppCommandLists{ commandList.Get() };
        //execute the array of command lists
        commandQueue->ExecuteCommandLists(ppCommandLists.size(),
            ppCommandLists.data());
        hr = commandQueue->Signal(fence[frameIndex].Get(), fenceValue[frameIndex]);
        assert(hr == S_OK);
        hr = swapChain->Present(0, 0);
        assert(hr == S_OK);
    }

    Context::Context(int w, int h, HWND hwnd)
    {
        HRESULT hr;
        //the factory is used to create DXGI objects.
        ComPtr<IDXGIFactory4> dxgiFactory = common::CreateDXGIFactory();
        //the IDXGIAdapter1 is equivalent to VkPhysicalDevice
        ComPtr<IDXGIAdapter1> adapter = common::FindAdapter(dxgiFactory); // adapters are the graphics card (this includes the embedded graphics on the motherboard)
#if defined(_DEBUG)
        //Debug Layer has to be created before the device or the device won't have debug enabled
        debugLayer = common::CreateDebugLayer();
        assert(debugLayer != nullptr);
#endif
        //ID3D12Device is equivalent do VkDevice
        device = common::CreateDevice(adapter);
#if defined(_DEBUG)
        device->QueryInterface(IID_PPV_ARGS(&debugDevice));
        assert(debugDevice != nullptr);
#endif
        //We'll need a command queue to run the commands, it's equivalent to vkCommandQueue
        commandQueue = common::CreateDirectCommandQueue(device, L"MainCommandQueue");
        //The swap chain, created with the size of the screenm using dxgi to fabricate the objects
        swapChain = common::CreateSwapChain(hwnd, w, h, FRAMEBUFFER_COUNT, true,
            commandQueue,
            dxgiFactory);
        frameIndex = swapChain->GetCurrentBackBufferIndex();
        //create a heap for render target view descriptors and get the size of it's descriptors. We have to get the
        //size because it can vary from device to device.
        rtvData = std::make_shared<common::RenderTargetViewData>(
            common::CreateRenderTargetViewDescriptorHeap(FRAMEBUFFER_COUNT, device),
            device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
            FRAMEBUFFER_COUNT
        );
        // now we create the render targets for the swap chain, linking the swap chain buffers to render target view descriptors
        swapChainRenderTargets = common::CreateRenderTargets(rtvData,
            swapChain, device);
        // We need a command allocator for each frame because if we use a single allocator each frame will mess with the other frames
        // allocations due to the fact that we need to reset the allocator before using it.
        commandAllocator = common::CreateCommandAllocators(FRAMEBUFFER_COUNT, device);
        // We are using single thread. If we were using multiple threads we'd need to have a list for each thread.
        hr = device->CreateCommandList(0,//default gpu 
            D3D12_COMMAND_LIST_TYPE_DIRECT, //type of commands - direct means that the commands can be executed by the gpu
            commandAllocator[0].Get(), //we need to specify an allocator to create the list, so we choose the first
            NULL, IID_PPV_ARGS(&commandList));
        assert(hr == S_OK);
        commandList->Close();
        //now we create the ring buffer for fences, one per frame
        assert(fence.size() == 0);
        fence.resize(FRAMEBUFFER_COUNT);
        fenceValue.resize(FRAMEBUFFER_COUNT);
        // create the fences
        for (int i = 0; i < FRAMEBUFFER_COUNT; i++)
        {
            hr = device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence[i]));
            assert(hr == S_OK);
            fenceValue[i] = 0; // set the initial fence value to 0
        }
    }

    ComPtr<ID3D12RootSignature> Context::CreateRootSignature(const std::wstring& name)
    {
        //create root signature
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0,//number of parameters
            nullptr,//parameter list
            0,//number of static samplers
            nullptr,//static samplers list
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT //flags
        );
        //We need this serialization step
        ID3DBlob* signature;
        HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &signature, nullptr);
        assert(hr == S_OK);
        ComPtr<ID3D12RootSignature> rootSignature = nullptr;
        hr = device->CreateRootSignature(0,
            signature->GetBufferPointer(), //the serialized data is used here
            signature->GetBufferSize(), //the serialized data is used here
            IID_PPV_ARGS(&rootSignature));
        rootSignature->SetName(name.c_str());
        return rootSignature;
    }




}
