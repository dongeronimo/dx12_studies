#include "pch.h"
#include "direct3d_context.h"
#include "../Common/d3d_utils.h"
#include "pipeline.h"
#include "view_projection.h"
#include "model_matrix.h"
#include "../Common/concatenate.h"
#include "../Common/input_layout_service.h"
using Microsoft::WRL::ComPtr;
using namespace common;
namespace transforms
{
    void Context::CreateShadowMapPipeline(ID3D12RootSignature* rootSig)
    {
        const std::wstring vertexShaderFileName = L"C:\\dev\\directx12\\x64\\Debug\\shadow_map_vs.cso";
        const std::wstring pixelShaderFileName = L"C:\\dev\\directx12\\x64\\Debug\\shadow_map_ps.cso";
        ///////SHADER LOADING///////
        std::filesystem::path cwd = std::filesystem::current_path();
        HRESULT hr;
        ID3DBlob* vertexShader;
        hr = D3DReadFileToBlob(vertexShaderFileName.c_str(), &vertexShader);
        assert(hr == S_OK);
        D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
        vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
        vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();
        //Load fragment shader bytecode into memory
        ID3DBlob* pixelShader;
        hr = D3DReadFileToBlob(pixelShaderFileName.c_str(), &pixelShader);
        assert(hr == S_OK);
        // fill out shader bytecode structure for pixel shader
        D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
        pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
        pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();
        ///////////////////////////////////
        std::vector< D3D12_INPUT_ELEMENT_DESC> inputLayout = common::input_layout_service::DefaultVertexDataAndInstanceId();
        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
        inputLayoutDesc.NumElements = inputLayout.size();
        inputLayoutDesc.pInputElementDescs = inputLayout.data();
        //////////////////////////////////
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // Root signature
        psoDesc.pRootSignature = rootSig;
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        psoDesc.InputLayout = inputLayoutDesc;
        // Rasterizer state - optimized for shadow mapping
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;  // Cull back faces
        psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
        psoDesc.RasterizerState.DepthBias = 100;                    
        psoDesc.RasterizerState.DepthBiasClamp = 0.0f;
        psoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
        psoDesc.RasterizerState.DepthClipEnable = TRUE;
        psoDesc.RasterizerState.MultisampleEnable = FALSE;
        psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
        psoDesc.RasterizerState.ForcedSampleCount = 0;
        psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend state - no blending needed for shadow maps
        psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
        psoDesc.BlendState.IndependentBlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // Depth stencil state - enable depth testing and writing
        psoDesc.DepthStencilState.DepthEnable = TRUE;
        psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        psoDesc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        psoDesc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        psoDesc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        psoDesc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

        // Sample description
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Render target formats - must match your CubeMapShadowMap format
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R32G32B32A32_FLOAT;  // Matches your cube map format
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;               // Matches your depth format

        // Primitive topology
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // Create the PSO
        hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&shadowMapPSO));
        shadowMapPSO->SetName(L"ShadowMapPipeline");
    }
    void Context::CreateFullscreenQuadPipeline(ID3D12RootSignature* rootSignature)
    {
        const std::wstring vertexShaderFileName = L"C:\\dev\\directx12\\x64\\Debug\\scene_offscreen_presentation_vs.cso";
        const std::wstring pixelShaderFileName = L"C:\\dev\\directx12\\x64\\Debug\\scene_offscreen_presentation_ps.cso";
        ///////SHADER LOADING///////
        std::filesystem::path cwd = std::filesystem::current_path();
        HRESULT hr;
        ID3DBlob* vertexShader;
        hr = D3DReadFileToBlob(vertexShaderFileName.c_str(), &vertexShader);
        assert(hr == S_OK);
        D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
        vertexShaderBytecode.BytecodeLength = vertexShader->GetBufferSize();
        vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();
        //Load fragment shader bytecode into memory
        ID3DBlob* pixelShader;
        hr = D3DReadFileToBlob(pixelShaderFileName.c_str(), &pixelShader);
        assert(hr == S_OK);
        // fill out shader bytecode structure for pixel shader
        D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
        pixelShaderBytecode.BytecodeLength = pixelShader->GetBufferSize();
        pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();
        ///////////////////////////////////

        // Create Pipeline State Object
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

        // Root signature
        psoDesc.pRootSignature = rootSignature;

        // Shaders
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader);
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader);

        // Input layout - EMPTY since we use SV_VertexID
        psoDesc.InputLayout = { nullptr, 0 };

        // Primitive topology
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // Render target format
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN; // No depth buffer needed

        // Sample description
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleDesc.Quality = 0;

        // Rasterizer state
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Don't cull for fullscreen quad

        // Blend state - usually no blending for final presentation
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;

        // Depth stencil state - disabled since no depth testing needed
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;

        // Sample mask
        psoDesc.SampleMask = UINT_MAX;

        // Create the PSO
        ComPtr<ID3D12PipelineState> pipelineState;
        hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));

        if (FAILED(hr))
        {
            throw std::runtime_error("");
        }
        fullscreenQuadPSO = pipelineState;
    }
    void Context::CreateStagingAndGPUBuffer(size_t size,
        Microsoft::WRL::ComPtr<ID3D12Resource>& _gpuBuffer,
        Microsoft::WRL::ComPtr<ID3D12Resource>& _stagingBuffer,
        D3D12_VERTEX_BUFFER_VIEW& _gpuBufferView, 
        const std::wstring& name)
    {
        assert(_gpuBuffer == nullptr);
        CD3DX12_HEAP_PROPERTIES stagingHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        device->CreateCommittedResource(
            &stagingHeapProperties, // a default heap
            D3D12_HEAP_FLAG_NONE, // no flags
            &resourceDesc, // resource description for a buffer
            D3D12_RESOURCE_STATE_COMMON,//the initial state of vertex buffer is D3D12_RESOURCE_STATE_COMMON. Before we use them i'll have to transition it to the correct state
            nullptr, // optimized clear value must be null for this type of resource. used for render targets and depth/stencil buffers
            IID_PPV_ARGS(&_gpuBuffer));
        // we can give resource heaps a name so when we debug with the graphics debugger we know what resource we are looking at
        _gpuBuffer->SetName(name.c_str());
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties = 
            CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        device->CreateCommittedResource(
            &uploadHeapProperties, // upload heap
            D3D12_HEAP_FLAG_NONE, // no flags
            &resourceDesc, // resource description for a buffer
            D3D12_RESOURCE_STATE_GENERIC_READ, // GPU will read from this buffer and copy its contents to the default heap
            nullptr,
            IID_PPV_ARGS(&_stagingBuffer));
        auto sb_name = Concatenate(name, "_staging");
        _stagingBuffer->SetName(sb_name.c_str());
        _gpuBufferView.BufferLocation = _gpuBuffer->GetGPUVirtualAddress();
        _gpuBufferView.StrideInBytes = size;
        _gpuBufferView.SizeInBytes = size;

    }
    void Context::CreateVertexBufferFromData(std::vector<Vertex>& _vertices, 
        Microsoft::WRL::ComPtr<ID3D12Resource>& _vertexBuffer, 
        D3D12_VERTEX_BUFFER_VIEW& _vertexBufferView, const std::wstring& name)
    {
        assert(_vertexBuffer == nullptr);
        //size IN BYTES of the buffer
        int vBufferSize = static_cast<int>(_vertices.size()) * sizeof(Vertex);
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
    void Context::SetCurrentOutputMergerTarget(CD3DX12_CPU_DESCRIPTOR_HANDLE& handle)
    {
        // set the render target for the output merger stage (the output of the pipeline)
        commandList->OMSetRenderTargets(1, &handle, FALSE, nullptr);
        this->rtvHandle = handle;
    }
    void Context::SetCurrentOutputMergerTarget()
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE _rtvHandle(
            rtvData->rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
            frameIndex, rtvData->rtvDescriptorSize);
        // set the render target for the output merger stage (the output of the pipeline)
        commandList->OMSetRenderTargets(1, &_rtvHandle, FALSE, nullptr);
        this->rtvHandle = _rtvHandle;
        
        // get a handle to the depth/stencil buffer
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(
            dsDescriptorHeap[frameIndex]->GetCPUDescriptorHandleForHeapStart());
        // set the render target for the output merger stage (the output of the pipeline)
        commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
    }
    void Context::ClearRenderTargetView(std::array<float, 4> rgba)
    {
        commandList->ClearDepthStencilView(
            dsDescriptorHeap[frameIndex]->GetCPUDescriptorHandleForHeapStart(),
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        commandList->ClearRenderTargetView(rtvHandle, rgba.data(), 0, nullptr);
    }
    void Context::BindRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature> rs,
        ViewProjection& viewProjectionData,
        ModelMatrix& modelMatricesData)
    {
        //bind the root signature
        commandList->SetGraphicsRootSignature(rs.Get());
        //bind the descriptor table (SRV for the structured buffer that holds the model matrices)
        ID3D12DescriptorHeap* h = modelMatricesData.DescriptorHeap(frameIndex).Get();
        commandList->SetDescriptorHeaps(1, &h);
        //Model Matrices are param #0
        commandList->SetGraphicsRootDescriptorTable(0, 
            modelMatricesData.DescriptorHeap(frameIndex)->GetGPUDescriptorHandleForHeapStart());
        // the root constants are in param #1
        // the constant buffer for view/proj data is in #2
        commandList->SetGraphicsRootConstantBufferView(2,
            viewProjectionData.GetConstantBuffer()->GetGPUVirtualAddress());
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
        commandQueue->ExecuteCommandLists(static_cast<UINT>(ppCommandLists.size()),
            ppCommandLists.data());
        hr = commandQueue->Signal(fence[frameIndex].Get(), fenceValue[frameIndex]);
        assert(hr == S_OK);
        hr = swapChain->Present(0, 0);
        assert(hr == S_OK);
    }

    void Context::CreateConstantBufferView(
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& descriptorHeap, 
        D3D12_CONSTANT_BUFFER_VIEW_DESC& cbvDesc,
        Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer,
        size_t constantBufferSize)
    {
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = static_cast<UINT>(constantBufferSize);

        // Create the descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = 1;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));

        // Create the CBV in the heap.
        device->CreateConstantBufferView(&cbvDesc, descriptorHeap->GetCPUDescriptorHandleForHeapStart());
    }

    void Context::Resize(int w, int h)
    {
        WaitAllFrames();

        //recreate render targets
        for (auto& x : swapChainRenderTargets) {
            x.ReleaseAndGetAddressOf();
        }
        swapChainRenderTargets.clear();
        swapChain->ResizeBuffers(FRAMEBUFFER_COUNT, w, h, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
        swapChainRenderTargets = common::CreateRenderTargets(rtvData, swapChain, device);
        //recreate depth buffer
        for (auto& x : dsDescriptorHeap) {
            x.ReleaseAndGetAddressOf();
        }
        dsDescriptorHeap.clear();
        dsDescriptorHeap.resize(FRAMEBUFFER_COUNT);
        for (auto& x : depthStencilBuffer) {
            x.ReleaseAndGetAddressOf();
        }
        depthStencilBuffer.clear();
        depthStencilBuffer.resize(FRAMEBUFFER_COUNT);
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;
        for (int i = 0; i < FRAMEBUFFER_COUNT; i++)
        {
            // create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
            dsvHeapDesc.NumDescriptors = 1;
            dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            HRESULT hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap[i]));
            assert(hr == S_OK);
            std::wstring name = Concatenate(L"depthStencilDescriptorHeap", i);
            dsDescriptorHeap[i]->SetName(name.c_str());
        }
        CD3DX12_HEAP_PROPERTIES depthStencilHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        CD3DX12_RESOURCE_DESC depthStencilResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, //format
            w, h, // w/h 
            1, //array size 
            1, //mip levels 
            1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        for (int i = 0; i < FRAMEBUFFER_COUNT; i++)
        {
            device->CreateCommittedResource(
                &depthStencilHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &depthStencilResourceDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depthOptimizedClearValue,
                IID_PPV_ARGS(&depthStencilBuffer[i])
            );
            std::wstring name = Concatenate(L"depthBuffer", i);
            depthStencilBuffer[i]->SetName(name.c_str());
            device->CreateDepthStencilView(depthStencilBuffer[i].Get(),
                &depthStencilDesc,
                dsDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());

        }
    }

    void Context::WaitAllFrames()
    {
        std::vector<HANDLE> fenceEvents(FRAMEBUFFER_COUNT);
        for (UINT i = 0; i < FRAMEBUFFER_COUNT; ++i) {
            const UINT64 currentFenceValue = fenceValue[i];
            commandQueue->Signal(fence[i].Get(), currentFenceValue);
            fenceValue[i]++;

            if (fence[i]->GetCompletedValue() < currentFenceValue) {
                fence[i]->SetEventOnCompletion(currentFenceValue, fenceEvents[i]);
                WaitForSingleObject(fenceEvents[i], INFINITE);
            }
        }
    }

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> Context::ResetFrame()
    {
        //wait until i can interact with this frame again
        WaitForPreviousFrame();
        //reset the allocator and the command list
        ResetCurrentCommandList();
        auto commandList = GetCommandList();
        return commandList;
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
        //create the depth-stencil buffer, one for each frame
        depthStencilBuffer.resize(FRAMEBUFFER_COUNT);
        dsDescriptorHeap.resize(FRAMEBUFFER_COUNT);
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;
        for (int i = 0; i < FRAMEBUFFER_COUNT; i++)
        {
            // create a depth stencil descriptor heap so we can get a pointer to the depth stencil buffer
            D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
            dsvHeapDesc.NumDescriptors = 1;
            dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            hr = device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsDescriptorHeap[i]));
            assert(hr == S_OK);
            std::wstring name = Concatenate(L"depthStencilDescriptorHeap", i);
            dsDescriptorHeap[i]->SetName(name.c_str());
        }
        CD3DX12_HEAP_PROPERTIES depthStencilHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        CD3DX12_RESOURCE_DESC depthStencilResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, //format
            w, h, // w/h 
            1, //array size 
            1, //mip levels 
            1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        for (int i = 0; i < FRAMEBUFFER_COUNT; i++)
        {
            device->CreateCommittedResource(
                &depthStencilHeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &depthStencilResourceDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &depthOptimizedClearValue,
                IID_PPV_ARGS(&depthStencilBuffer[i])
            );
            std::wstring name = Concatenate(L"depthBuffer", i);
            depthStencilBuffer[i]->SetName(name.c_str());
            device->CreateDepthStencilView(depthStencilBuffer[i].Get(),
                &depthStencilDesc,
                dsDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());

        }

    }
    /// <summary>
    /// The default root signature, as used by transforms_vertex_shader.
    /// 0: the srv at t0 (per object data)
    /// 1: the root constant for object id at b0
    /// 2: the srv at t1 (per frame data)
    /// </summary>
    /// <param name="name"></param>
    /// <returns></returns>
    ComPtr<ID3D12RootSignature> Context::CreateUnlitDebugRootSignature(const std::wstring& name)
    {
        //the table of root signature parameters
        std::array<CD3DX12_ROOT_PARAMETER, 3> rootParams;
        //1) PerObjectData 
        CD3DX12_DESCRIPTOR_RANGE perObjectDataSRVRange(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, //it's a shader resource view 
            1, 
            0); //register t0
        rootParams[0].InitAsDescriptorTable(1, &perObjectDataSRVRange);
        //2) RootConstants - holds the objectId push constant
        rootParams[1].InitAsConstants(1, //inits one dword (that'll carry the instanceId)
            0);//register b0
        //3) PerFrameData (view/projection data)
        CD3DX12_DESCRIPTOR_RANGE perFrameDataSRVRange(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV, //it's a shader resource view 
            1,
            1); //register t1
        rootParams[2].InitAsDescriptorTable(1, &perFrameDataSRVRange);


        //create root signature
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(static_cast<UINT>(rootParams.size()),//number of parameters
            rootParams.data(),//parameter list
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

    Microsoft::WRL::ComPtr<ID3D12RootSignature> Context::CreateSimpleLightingRootSignature(const std::wstring& name)
    {
        //the table of root signature parameters
        std::array<CD3DX12_ROOT_PARAMETER, 5> rootParams; // Increased from 4 to 5

        //1) PerObjectData 
        CD3DX12_DESCRIPTOR_RANGE perObjectDataSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); //register t0
        rootParams[0].InitAsDescriptorTable(1, &perObjectDataSRVRange);

        //2) RootConstants - holds the objectId push constant
        rootParams[1].InitAsConstants(1, 0);//register b0

        //3) PerFrameData (view/projection data)
        CD3DX12_DESCRIPTOR_RANGE perFrameDataSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); //register t1
        rootParams[2].InitAsDescriptorTable(1, &perFrameDataSRVRange);

        //4) lighting data
        CD3DX12_DESCRIPTOR_RANGE lightingSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2);
        rootParams[3].InitAsDescriptorTable(1, &lightingSRVRange);

        //5) Shadow maps - NEW
        CD3DX12_DESCRIPTOR_RANGE shadowMapsSRVRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MAX_LIGHTS*6, 3); // 8 shadow maps starting at t3
        rootParams[4].InitAsDescriptorTable(1, &shadowMapsSRVRange, D3D12_SHADER_VISIBILITY_PIXEL);

        // Static sampler for shadow maps - NEW
        CD3DX12_STATIC_SAMPLER_DESC staticSampler;
        staticSampler.Init(0,                                    // s0
            D3D12_FILTER_MIN_MAG_MIP_LINEAR,      // Linear filtering
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,     // Clamp U
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,     // Clamp V  
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,     // Clamp W
            0.0f,                                  // MipLODBias
            16,                                    // MaxAnisotropy
            D3D12_COMPARISON_FUNC_NEVER,          // ComparisonFunc
            D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE, // BorderColor
            0.0f,                                  // MinLOD
            D3D12_FLOAT32_MAX,                     // MaxLOD
            D3D12_SHADER_VISIBILITY_PIXEL,        // Visibility
            0);                                    // RegisterSpace

        //create root signature
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(static_cast<UINT>(rootParams.size()),//number of parameters
            rootParams.data(),//parameter list
            1,//number of static samplers - CHANGED from 0 to 1
            &staticSampler,//static samplers list - CHANGED from nullptr
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
    Microsoft::WRL::ComPtr<ID3D12RootSignature> Context::CreateQuadRenderRootSignature()
    {
        using namespace Microsoft::WRL;
        // Root parameter for just the texture (SRV)
        CD3DX12_ROOT_PARAMETER1 rootParameters[1];
        // Parameter 0: Texture2D (SRV) - register(t0)
        CD3DX12_DESCRIPTOR_RANGE1 srvRange;
        srvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
        rootParameters[0].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

        // Static sampler (more efficient than descriptor table)
        CD3DX12_STATIC_SAMPLER_DESC staticSampler;
        staticSampler.Init(
            0, // register s0
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // Linear filtering
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
            0.0f, // mip LOD bias
            16,   // max anisotropy
            D3D12_COMPARISON_FUNC_NEVER,
            D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
            0.0f, // min LOD
            D3D12_FLOAT32_MAX, // max LOD
            D3D12_SHADER_VISIBILITY_PIXEL
        );

        // Create root signature descriptor
        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init_1_1(
            _countof(rootParameters),
            rootParameters,
            1, &staticSampler, // Use static sampler
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
        );

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        HRESULT hr = D3DX12SerializeVersionedRootSignature(
            &rootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1_1,
            &signature,
            &error
        );

        if (FAILED(hr))
        {
            if (error)
            {
                OutputDebugStringA(static_cast<char*>(error->GetBufferPointer()));
            }
            return nullptr;
        }

        ComPtr<ID3D12RootSignature> rootSignature;
        hr = device->CreateRootSignature(
            0,
            signature->GetBufferPointer(),
            signature->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature)
        );
        rootSignature->SetName(L"QuadRenderRootSignature");
        return rootSignature;
    }

    Microsoft::WRL::ComPtr<ID3D12RootSignature> Context::CreateShadowMapRootSignature()
    {
        //the table of root signature parameters
        std::array<CD3DX12_ROOT_PARAMETER, 4> rootParams;
        // Parameter 0: Root constants (b0) - objectId
        rootParams[0].InitAsConstants(
            1,          // Number of 32-bit constants (uint objectId = 1 constant)
            0,          // Register (b0)
            0           // Register space
        );
        // Parameter 1: Root constants (b1) - Shadow data id
        rootParams[1].InitAsConstants(
            1,          // Number of 32-bit constants (uint objectId = 1 constant)
            1,          // Register (b1)
            0           // Register space
        );

        // Parameter 2: Structured buffer (t0) - PerObjectData
        CD3DX12_DESCRIPTOR_RANGE srvRangeForPerObjectData = {};
        srvRangeForPerObjectData.Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,    // Range type
            1,                                   // Number of descriptors
            0,                                   // Base register (t0)
            0                                    // Register space
        );
        rootParams[2].InitAsDescriptorTable(
            1,              // Number of descriptor ranges
            &srvRangeForPerObjectData,      // Descriptor ranges
            D3D12_SHADER_VISIBILITY_VERTEX  // Only vertex shader needs this
        );
        // Parameter 3: Structured buffer (t1) - ShadowData
        CD3DX12_DESCRIPTOR_RANGE srvRangeForShadowData = {};
        srvRangeForShadowData.Init(
            D3D12_DESCRIPTOR_RANGE_TYPE_SRV,    // Range type
            1,                                   // Number of descriptors
            1,                                   // Base register (t0)
            0                                    // Register space
        );
        rootParams[3].InitAsDescriptorTable(
            1,              // Number of descriptor ranges
            &srvRangeForShadowData,      // Descriptor ranges
            D3D12_SHADER_VISIBILITY_ALL  // Only vertex shader needs this
        );


        //create root signature
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(static_cast<UINT>(rootParams.size()),//number of parameters
            rootParams.data(),//parameter list
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
        rootSignature->SetName(L"Shadow Map root sig");
        return rootSignature;
    }




}
