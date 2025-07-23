#pragma once
#include "pch.h"

namespace transforms {
    /// <summary>
    /// A shared heap, for shader visible resources. I need this because commandList->SetDescriptorHeaps
    /// demands that all shader resource views come from the same heap.
    /// </summary>
    class SharedDescriptorHeapV2
    {
    public:
        SharedDescriptorHeapV2(ID3D12Device* device, UINT numDescriptors);
        std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> AllocateDescriptor();
        ID3D12DescriptorHeap* GetHeap() {
            return heap;
        }
    private:
        void ExpandPool();
        ID3D12DescriptorHeap* heap;
        ID3D12Device* device;
        D3D12_CPU_DESCRIPTOR_HANDLE cpuStart;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart;
        UINT descriptorSize;
        UINT currentCapacity;
        UINT nextFreeIndex;
    };
}   

