#pragma once
#include "pch.h"
using Microsoft::WRL::ComPtr;
namespace transforms {
    class SharedDescriptorHeapV2;
    class RtvDsvDescriptorHeapManager;

    // Structure that matches the HLSL cbuffer ShadowMapConstants
    struct alignas(16) ShadowMapConstants
    {
        DirectX::XMFLOAT4X4 viewMatrix;
        DirectX::XMFLOAT4X4 projMatrix;
        DirectX::XMFLOAT3 lightPosition;
        float farPlane;
    };

    class CubeMapShadowMap
    {
    private:
        float m_farPlane = 500.0f;
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
        static bool shadowTableIsCreated;

    public:
        const UINT m_id;
        CubeMapShadowMap(std::wstring& name, UINT id);
        ~CubeMapShadowMap() = default;
        float GetFarPlane()const { return m_farPlane; }
        void TransitionToRenderTarget(ID3D12GraphicsCommandList* commandList, int frameIndex);
        void TransitionToPixelShaderResource(ID3D12GraphicsCommandList* commandList, int frameIndex);
        void SetAsRenderTarget(ID3D12GraphicsCommandList* commandList, int faceIndex);
        void Clear(ID3D12GraphicsCommandList* commandList, int faceIndex, std::array<float, 4> clearColor);
        // Initialize the cube map shadow map
        bool Initialize(ID3D12Device* device,
            RtvDsvDescriptorHeapManager* descriptorManager,
            SharedDescriptorHeapV2* srvHeapManager,
            UINT resolution = SHADOW_MAP_SIZE,
            DXGI_FORMAT colorFormat = DXGI_FORMAT_R32G32B32A32_FLOAT,
            DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT);
        // Get handles for rendering
        const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6>& GetRTVHandles() const { return m_rtvHandles; }
        const std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 6>& GetDSVHandle() const { return m_dsvHandles; }
        D3D12_GPU_DESCRIPTOR_HANDLE GetSRV_GPUHandle() const { return m_srvHandle.second; }
        D3D12_CPU_DESCRIPTOR_HANDLE GetSRV_CPUHandle() const { return m_srvHandle.first; }
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

