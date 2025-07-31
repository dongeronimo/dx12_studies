#include "shared_descriptor_heap_v2.h"

transforms::SharedDescriptorHeapV2::SharedDescriptorHeapV2(ID3D12Device* device, 
    UINT numDescriptors):currentCapacity(numDescriptors), device(device)
{
    // Create GPU-visible heap
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));

    // Get starting handles and size
    cpuStart = heap->GetCPUDescriptorHandleForHeapStart();
    gpuStart = heap->GetGPUDescriptorHandleForHeapStart();
    descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    nextFreeIndex = 0;

    //there are some SRVs that have to be pre-allocated, like the ones for shadow maps.
    shadowMapDescriptorRangeStart = AllocateDescriptorRange(MAX_LIGHTS * 6);
}

std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> transforms::SharedDescriptorHeapV2::DescriptorForShadowMap(UINT idx)
{
    if (idx >= MAX_LIGHTS) {
        throw std::out_of_range("Shadow map index out of range");
    }
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = shadowMapDescriptorRangeStart.first;
    cpuHandle.ptr += (idx * descriptorSize);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = shadowMapDescriptorRangeStart.second;
    gpuHandle.ptr += (idx * descriptorSize);
    return {cpuHandle, gpuHandle };
}

std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> transforms::SharedDescriptorHeapV2::AllocateDescriptor()
{
    // Check if we need to expand
    if (nextFreeIndex >= currentCapacity) {
        ExpandPool();
    }

    UINT index = nextFreeIndex++;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cpuStart;
    cpuHandle.ptr += (index * descriptorSize);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = gpuStart;
    gpuHandle.ptr += (index * descriptorSize);

    return { cpuHandle, gpuHandle };
}

std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> transforms::SharedDescriptorHeapV2::AllocateDescriptorRange(UINT count)
{
    if (nextFreeIndex + count > currentCapacity) {
        ExpandPool();  // You might want to make ExpandPool smarter for large count
    }

    UINT index = nextFreeIndex;
    nextFreeIndex += count;

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = cpuStart;
    cpuHandle.ptr += (index * descriptorSize);

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = gpuStart;
    gpuHandle.ptr += (index * descriptorSize);

    return { cpuHandle, gpuHandle };
}

void transforms::SharedDescriptorHeapV2::ExpandPool()
{
    UINT newCapacity = currentCapacity * 2; // Double the size

    // Create new larger heap
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = newCapacity;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ID3D12DescriptorHeap* newHeap;
    device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&newHeap));

    // Copy existing descriptors to new heap
    D3D12_CPU_DESCRIPTOR_HANDLE oldCpuStart = heap->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE newCpuStart = newHeap->GetCPUDescriptorHandleForHeapStart();

    device->CopyDescriptorsSimple(
        nextFreeIndex,  // Copy all used descriptors
        newCpuStart,
        oldCpuStart,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
    );

    // Update to use new heap
    heap->Release();
    heap = newHeap;
    cpuStart = newCpuStart;
    gpuStart = newHeap->GetGPUDescriptorHandleForHeapStart();
    currentCapacity = newCapacity;
}
