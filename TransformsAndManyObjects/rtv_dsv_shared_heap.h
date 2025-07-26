#pragma once
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
namespace transforms {
    class RtvDsvDescriptorHeapManager
    {
    private:
        ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
        ComPtr<ID3D12DescriptorHeap> m_dsvHeap;

        UINT m_rtvDescriptorSize;
        UINT m_dsvDescriptorSize;

        UINT m_rtvCount;
        UINT m_dsvCount;
        UINT m_rtvUsed;
        UINT m_dsvUsed;

    public:
        RtvDsvDescriptorHeapManager() :
            m_rtvDescriptorSize(0),
            m_dsvDescriptorSize(0),
            m_rtvCount(0),
            m_dsvCount(0),
            m_rtvUsed(0),
            m_dsvUsed(0) {}

        // Initialize the heaps
        bool Initialize(ID3D12Device* device, UINT rtvCount = 256, UINT dsvCount = 64)
        {
            // Create RTV heap
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.NumDescriptors = rtvCount;
            rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            if (FAILED(device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&m_rtvHeap))))
                return false;

            // Create DSV heap
            D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
            dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            dsvDesc.NumDescriptors = dsvCount;
            dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            if (FAILED(device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap))))
                return false;

            // Get descriptor sizes
            m_rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            m_dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

            m_rtvCount = rtvCount;
            m_dsvCount = dsvCount;

            return true;
        }

        // Get heap pointers for sharing
        ID3D12DescriptorHeap* GetRTVHeap() const { return m_rtvHeap.Get(); }
        ID3D12DescriptorHeap* GetDSVHeap() const { return m_dsvHeap.Get(); }

        // Allocate descriptors
        D3D12_CPU_DESCRIPTOR_HANDLE AllocateRTV()
        {
            if (m_rtvUsed >= m_rtvCount)
                return {}; // Invalid handle

            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += m_rtvUsed * m_rtvDescriptorSize;
            m_rtvUsed++;
            return handle;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE AllocateDSV()
        {
            if (m_dsvUsed >= m_dsvCount)
                return {}; // Invalid handle

            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += m_dsvUsed * m_dsvDescriptorSize;
            m_dsvUsed++;
            return handle;
        }

        // Get handles at specific indices
        D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT index) const
        {
            if (index >= m_rtvCount)
                return {};

            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += index * m_rtvDescriptorSize;
            return handle;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(UINT index) const
        {
            if (index >= m_dsvCount)
                return {};

            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += index * m_dsvDescriptorSize;
            return handle;
        }

        // Reset allocation counters (doesn't clear the descriptors)
        void Reset()
        {
            m_rtvUsed = 0;
            m_dsvUsed = 0;
        }

        // Get usage info
        UINT GetRTVUsed() const { return m_rtvUsed; }
        UINT GetDSVUsed() const { return m_dsvUsed; }
        UINT GetRTVCapacity() const { return m_rtvCount; }
        UINT GetDSVCapacity() const { return m_dsvCount; }
    };
}