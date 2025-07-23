#pragma once
#include "pch.h"
#include "per_object_data.h"
#include "direct3d_context.h"
#include "../Common/concatenate.h"
#include "shared_descriptor_heap_v2.h"
namespace transforms
{
    class Context;

    //class SharedDescriptorHeap {
    //public:
    //    SharedDescriptorHeap(ID3D12Device* device, UINT descriptorCount)
    //        : descriptorCount(descriptorCount)
    //    {
    //        descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    //        heap.resize(FRAMEBUFFER_COUNT);
    //        cpuStart.resize(FRAMEBUFFER_COUNT);
    //        gpuStart.resize(FRAMEBUFFER_COUNT);
    //        for (UINT frame = 0; frame < FRAMEBUFFER_COUNT; ++frame) {
    //            D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    //            desc.NumDescriptors = descriptorCount;
    //            desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    //            desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    //            desc.NodeMask = 0;
    //            HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap[frame]));
    //            assert(SUCCEEDED(hr));
    //            
    //            cpuStart[frame] = heap[frame]->GetCPUDescriptorHandleForHeapStart();
    //            gpuStart[frame] = heap[frame]->GetGPUDescriptorHandleForHeapStart();
    //        }
    //    }
    //    ID3D12DescriptorHeap* GetHeap(UINT frameIndex) const {
    //        assert(frameIndex < FRAMEBUFFER_COUNT);
    //        return heap[frameIndex].Get();
    //    }
    //    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT frameIndex, UINT index) const {
    //        assert(frameIndex < FRAMEBUFFER_COUNT);
    //        assert(index < descriptorCount);
    //        D3D12_CPU_DESCRIPTOR_HANDLE handle = cpuStart[frameIndex];
    //        handle.ptr += index * descriptorSize;
    //        return handle;
    //    }
    //    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT frameIndex, UINT index) const {
    //        assert(frameIndex < FRAMEBUFFER_COUNT);
    //        assert(index < descriptorCount);
    //        D3D12_GPU_DESCRIPTOR_HANDLE handle = gpuStart[frameIndex];
    //        handle.ptr += index * descriptorSize;
    //        return handle;
    //    }
    //    UINT GetDescriptorSize() const {
    //        return descriptorSize;
    //    }
    //private:
    //    std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> heap;
    //    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> cpuStart;
    //    std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> gpuStart;
    //    UINT descriptorCount = 0;
    //    UINT descriptorSize = 0;
    //};

    template<typename model_data_t>
    class UniformBufferForSRVs
    {
    public:
        UniformBufferForSRVs(Context& ctx, 
            SharedDescriptorHeapV2* sharedHeap,
            UINT descriptorIndex, 
            UINT maxNumberOfObjs = MAX_NUMBER_OF_OBJ)
            : sharedDescriptorHeap(sharedHeap), srvDescriptorIndex(descriptorIndex)
        {
            //structured buffers do not need alignment to 256 bytes
            bufferSize = sizeof(model_data_t) * maxNumberOfObjs;
            structuredBuffer.resize(FRAMEBUFFER_COUNT);
            uploadBuffer.resize(FRAMEBUFFER_COUNT);
            mappedData.resize(FRAMEBUFFER_COUNT);
            firstTimeUse.resize(FRAMEBUFFER_COUNT);
            gpuDescriptorHandle.resize(FRAMEBUFFER_COUNT);
            cpuDescriptorHandle.resize(FRAMEBUFFER_COUNT);
            for (UINT i = 0; i < FRAMEBUFFER_COUNT; i++) {
                firstTimeUse[i] = true;

                // 1. Create GPU and upload buffer
                Microsoft::WRL::ComPtr<ID3D12Resource> _gpuBuffer;
                Microsoft::WRL::ComPtr<ID3D12Resource> _stagingBuffer;
                D3D12_VERTEX_BUFFER_VIEW _gpuBufferView; // Unused for SRV, but required by your function
                auto name = Concatenate("PerObjectUniformBuffer", i);
                ctx.CreateStagingAndGPUBuffer(bufferSize, _gpuBuffer, _stagingBuffer,
                    _gpuBufferView, name);

                // 2. Create SRV in the shared descriptor heap
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                srvDesc.Format = DXGI_FORMAT_UNKNOWN;
                srvDesc.Buffer.FirstElement = 0;
                srvDesc.Buffer.NumElements = maxNumberOfObjs;
                srvDesc.Buffer.StructureByteStride = sizeof(model_data_t);
                srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

                auto [cpuHandle, gpuHandle] = sharedDescriptorHeap->AllocateDescriptor();
                // Create SRV in shared heap at the assigned index
                ctx.GetDevice()->CreateShaderResourceView(_gpuBuffer.Get(), &srvDesc, cpuHandle);
                cpuDescriptorHandle[i] = cpuHandle;
                gpuDescriptorHandle[i] = gpuHandle;
                // 3. Map staging buffer
                void* mappedPtr = nullptr;
                CD3DX12_RANGE readRange(0, 0); // We won't read from it
                _stagingBuffer->Map(0, &readRange, &mappedPtr);
                mappedData[i] = mappedPtr;

                // 4. Store buffers
                structuredBuffer[i] = _gpuBuffer;
                uploadBuffer[i] = _stagingBuffer;
            }
        }

        // Returns pointer to mapped per-frame upload buffer
        void* GetMappedData(UINT frameIndex) const {
            return mappedData[frameIndex];
        }

        // Returns GPU descriptor handle to bind with SetGraphicsRootDescriptorTable
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT frameIndex) const {
            return gpuDescriptorHandle[frameIndex];
        }

        // Returns shared heap pointer for SetDescriptorHeaps
        ID3D12DescriptorHeap* GetSharedHeap(UINT frameIndex) const {
            return sharedDescriptorHeap->GetHeap(frameIndex);
        }

        // Returns GPU buffer resource (for copy or inspection)
        ID3D12Resource* GetGPUBuffer(UINT frameIndex) const {
            return structuredBuffer[frameIndex].Get();
        }

        // Returns upload buffer resource
        ID3D12Resource* GetUploadBuffer(UINT frameIndex) const {
            return uploadBuffer[frameIndex].Get();
        }

        UINT64 GetAlignedBufferSize() const {
            return bufferSize;
        }

        UINT GetDescriptorIndex() const {
            return srvDescriptorIndex;
        }

        void CopyToGPU(UINT frameIndex, ID3D12GraphicsCommandList* commandList) {
            assert(commandList);
            auto* gpuBuffer = structuredBuffer[frameIndex].Get();
            auto* upload = uploadBuffer[frameIndex].Get();
            if (firstTimeUse[frameIndex] == true) {
                // COMMON -> COPY_DEST
                CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
                    gpuBuffer,
                    D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_STATE_COPY_DEST
                );
                commandList->ResourceBarrier(1, &toCopyDest);
                firstTimeUse[frameIndex] = false;
            }
            else {
                // NON_PIXEL_SHADER_RESOURCE -> COPY_DEST
                CD3DX12_RESOURCE_BARRIER toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
                    gpuBuffer,
                    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                    D3D12_RESOURCE_STATE_COPY_DEST
                );
                commandList->ResourceBarrier(1, &toCopyDest);
            }
            // Copy from staging to GPU buffer
            commandList->CopyBufferRegion(
                gpuBuffer,
                0,
                upload,
                0,
                bufferSize
            );

            // COPY_DEST -> NON_PIXEL_SHADER_RESOURCE (for SRV usage in shader)
            CD3DX12_RESOURCE_BARRIER toShaderRead = CD3DX12_RESOURCE_BARRIER::Transition(
                gpuBuffer,
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
            commandList->ResourceBarrier(1, &toShaderRead);
        }

        void SetValue(UINT frameIndex, UINT id, const model_data_t& data) {
            void* dst = mappedData[frameIndex];
            // Copy data to the correct offset in the buffer
            model_data_t* typedDst = static_cast<model_data_t*>(dst);
            typedDst[id] = data;
        }

    private:
        std::vector<bool> firstTimeUse;
        UINT64 bufferSize;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> structuredBuffer;
        std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> uploadBuffer;
        std::vector<void*> mappedData;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> cpuDescriptorHandle;
        std::vector< D3D12_GPU_DESCRIPTOR_HANDLE> gpuDescriptorHandle;
        // Reference to shared descriptor heap and our assigned index
        SharedDescriptorHeapV2* sharedDescriptorHeap;
        UINT srvDescriptorIndex;
    };
}
