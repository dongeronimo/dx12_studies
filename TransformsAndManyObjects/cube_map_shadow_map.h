#pragma once
#include "pch.h"
using Microsoft::WRL::ComPtr;
namespace transforms {
    class SharedDescriptorHeapV2;
    class RtvDsvDescriptorHeapManager;

    // Structure that matches the HLSL cbuffer ShadowMapConstants
    struct ShadowMapConstants
    {
        DirectX::XMFLOAT4X4 viewMatrix;
        DirectX::XMFLOAT4X4 projMatrix;
        DirectX::XMFLOAT3 lightPosition;
        float padding;
    };

    // You need to create and manage a constant buffer for shadow map constants
    class ShadowMapConstantBuffer
    {
    private:
        ComPtr<ID3D12Resource> m_constantBuffer;
        UINT8* m_mappedData;

    public:
        bool Initialize(ID3D12Device* device)
        {
            // Create upload heap for constant buffer
            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Width = (sizeof(ShadowMapConstants) + 255) & ~255; // Align to 256 bytes
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc.Count = 1;
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            HRESULT hr = device->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&m_constantBuffer)
            );

            if (FAILED(hr))
                return false;

            // Map the constant buffer (keep it mapped for the lifetime)
            hr = m_constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedData));
            return SUCCEEDED(hr);
        }

        void UpdateConstants(const ShadowMapConstants& constants)
        {
            memcpy(m_mappedData, &constants, sizeof(ShadowMapConstants));
        }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const
        {
            return m_constantBuffer->GetGPUVirtualAddress();
        }
    };


    class CubeMapShadowMap
    {
    private:
        ComPtr<ID3D12Resource> m_cubeMapTexture;
        ComPtr<ID3D12Resource> m_depthBuffer;

        // Descriptor handles
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6> m_rtvHandles; // One RTV per cube face
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6> m_dsvHandles;
        std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> m_srvHandle; // SRV for sampling

        // Shadow map properties
        UINT m_resolution;
        DXGI_FORMAT m_colorFormat;
        DXGI_FORMAT m_depthFormat;

        // References to descriptor managers (not owned)
        RtvDsvDescriptorHeapManager* m_descriptorManager;
        SharedDescriptorHeapV2* m_srvHeapManager;

        // View matrices for each cube face (for shadow rendering)
        std::array<DirectX::XMMATRIX, 6> m_viewMatrices;
        DirectX::XMMATRIX m_projectionMatrix;
        std::wstring m_name;
    public:
        CubeMapShadowMap(std::wstring& name);
        ~CubeMapShadowMap() = default;
        void TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList, int frameIndex);
        void TransitionToPixelShaderResource(ID3D12GraphicsCommandList* commandList, int frameIndex);
        void SetAsRenderTarget(ID3D12GraphicsCommandList* commandList, int faceIndex);
        void Clear(ID3D12GraphicsCommandList* commandList, int faceIndex, std::array<float, 4> clearColor);
        // Initialize the cube map shadow map
        bool Initialize(ID3D12Device* device,
            RtvDsvDescriptorHeapManager* descriptorManager,
            SharedDescriptorHeapV2* srvHeapManager,
            UINT resolution = 1024,
            DXGI_FORMAT colorFormat = DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT);
        // Get handles for rendering
        const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6>& GetRTVHandles() const { return m_rtvHandles; }
        const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6>& GetDSVHandle() const { return m_dsvHandles; }
        D3D12_GPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return m_srvHandle.second; }

        // Get resources
        ID3D12Resource* GetCubeMapTexture() const { return m_cubeMapTexture.Get(); }
        ID3D12Resource* GetDepthBuffer() const { return m_depthBuffer.Get(); }

        // Get properties
        UINT GetResolution() const { return m_resolution; }
        DXGI_FORMAT GetColorFormat() const { return m_colorFormat; }
        DXGI_FORMAT GetDepthFormat() const { return m_depthFormat; }

        // Get view/projection matrices for shadow rendering
        DirectX::XMMATRIX GetViewMatrix(UINT faceIndex) const
        {
            return (faceIndex < 6) ? m_viewMatrices[faceIndex] : DirectX::XMMatrixIdentity();
        }
        DirectX::XMMATRIX GetProjectionMatrix() const { return m_projectionMatrix; }

        // Update light position (recalculates view matrices)
        void UpdateLightPosition(const DirectX::XMFLOAT3& lightPos)
        {
            SetupViewMatrices(lightPos);
        }

    private:
        bool CreateCubeMapTexture(ID3D12Device* device);

        bool CreateDepthBuffer(ID3D12Device* device);

        bool CreateRenderTargetViews(ID3D12Device* device);

        bool CreateDepthStencilView(ID3D12Device* device);

        bool CreateShaderResourceView(ID3D12Device* device);

        void SetupViewMatrices(const DirectX::XMFLOAT3& lightPos = DirectX::XMFLOAT3(0, 0, 0));
    };
}

