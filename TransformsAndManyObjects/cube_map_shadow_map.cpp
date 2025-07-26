#include "pch.h"
#include "cube_map_shadow_map.h"
#include "shared_descriptor_heap_v2.h"
#include "rtv_dsv_shared_heap.h"
#include "../Common/concatenate.h"
namespace transforms {
    CubeMapShadowMap::CubeMapShadowMap(std::wstring& name) :
        m_resolution(0),
        m_colorFormat(DXGI_FORMAT_R32G32B32A32_FLOAT),
        m_depthFormat(DXGI_FORMAT_D32_FLOAT),
        m_descriptorManager(nullptr),
        m_name(name),
        m_srvHeapManager(nullptr) {
        // Initialize handles to invalid values
        for (auto& handle : m_rtvHandles)
        {
            handle.ptr = 0;
        }
        m_dsvHandle.ptr = 0;
        m_srvHandle.first.ptr = 0;
        m_srvHandle.second.ptr = 0;
    }
    void CubeMapShadowMap::SetupViewMatrices(const DirectX::XMFLOAT3& lightPos)
    {
        using namespace DirectX;

        XMVECTOR eyePos = XMLoadFloat3(&lightPos);

        // Cube face directions and up vectors
        struct CubeFace
        {
            XMVECTOR target;
            XMVECTOR up;
        };

        std::array<CubeFace, 6> faces = { {
            { XMVectorSet(1, 0, 0, 0),  XMVectorSet(0, 1, 0, 0) },  // +X
            { XMVectorSet(-1, 0, 0, 0), XMVectorSet(0, 1, 0, 0) },  // -X
            { XMVectorSet(0, 1, 0, 0),  XMVectorSet(0, 0, -1, 0) }, // +Y
            { XMVectorSet(0, -1, 0, 0), XMVectorSet(0, 0, 1, 0) },  // -Y
            { XMVectorSet(0, 0, 1, 0),  XMVectorSet(0, 1, 0, 0) },  // +Z
            { XMVectorSet(0, 0, -1, 0), XMVectorSet(0, 1, 0, 0) }   // -Z
        } };

        for (UINT i = 0; i < 6; ++i)
        {
            XMVECTOR target = XMVectorAdd(eyePos, faces[i].target);
            m_viewMatrices[i] = XMMatrixLookAtLH(eyePos, target, faces[i].up);
        }
    }

    bool CubeMapShadowMap::CreateShaderResourceView(ID3D12Device* device)
    {
        m_srvHandle = m_srvHeapManager->AllocateDescriptor();
        if (m_srvHandle.first.ptr == 0)
            return false;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = m_colorFormat;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.TextureCube.MipLevels = 1;
        srvDesc.TextureCube.MostDetailedMip = 0;

        device->CreateShaderResourceView(m_cubeMapTexture.Get(), &srvDesc, m_srvHandle.first);
        return true;
    }
    bool CubeMapShadowMap::CreateDepthStencilView(ID3D12Device* device)
    {
        m_dsvHandle = m_descriptorManager->AllocateDSV();
        if (m_dsvHandle.ptr == 0)
            return false;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = m_depthFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;

        device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvDesc, m_dsvHandle);
        return true;
    }

    bool CubeMapShadowMap::CreateRenderTargetViews(ID3D12Device* device)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = m_colorFormat;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = 0;
        rtvDesc.Texture2DArray.ArraySize = 1;

        // Create one RTV per cube face
        for (UINT i = 0; i < 6; ++i)
        {
            m_rtvHandles[i] = m_descriptorManager->AllocateRTV();
            if (m_rtvHandles[i].ptr == 0)
                return false;

            rtvDesc.Texture2DArray.FirstArraySlice = i;
            device->CreateRenderTargetView(m_cubeMapTexture.Get(), &rtvDesc, m_rtvHandles[i]);
        }

        return true;
    }

    bool CubeMapShadowMap::CreateDepthBuffer(ID3D12Device* device)
    {
        D3D12_RESOURCE_DESC depthDesc = {};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = m_resolution;
        depthDesc.Height = m_resolution;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.Format = m_depthFormat;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = m_depthFormat;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        bool r = SUCCEEDED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(&m_depthBuffer)
        ));
        auto n = Concatenate(m_name, "DepthBuffer");
        m_depthBuffer->SetName(n.c_str());
        return r;
    }

    bool CubeMapShadowMap::CreateCubeMapTexture(ID3D12Device* device)
    {
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Width = m_resolution;
        textureDesc.Height = m_resolution;
        textureDesc.DepthOrArraySize = 6; // 6 faces for cube map
        textureDesc.MipLevels = 1;
        textureDesc.Format = m_colorFormat;
        textureDesc.SampleDesc.Count = 1;
        textureDesc.SampleDesc.Quality = 0;
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = m_colorFormat;
        clearValue.Color[0] = 1.0f; // Clear to white (max depth for shadow maps)
        clearValue.Color[1] = 1.0f;
        clearValue.Color[2] = 1.0f;
        clearValue.Color[3] = 1.0f;

        bool r = SUCCEEDED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            &clearValue,
            IID_PPV_ARGS(&m_cubeMapTexture)
        ));
        auto n = Concatenate(m_name, "CubeMapTexture");
        m_cubeMapTexture->SetName(n.c_str());
        return r;
    }

    bool CubeMapShadowMap::Initialize(ID3D12Device* device,
        RtvDsvDescriptorHeapManager* descriptorManager,
        SharedDescriptorHeapV2* srvHeapManager,
        UINT resolution,
        DXGI_FORMAT colorFormat,
        DXGI_FORMAT depthFormat)
    {
        m_resolution = resolution;
        m_colorFormat = colorFormat;
        m_depthFormat = depthFormat;
        m_descriptorManager = descriptorManager;
        m_srvHeapManager = srvHeapManager;

        // Create cube map texture (color target)
        if (!CreateCubeMapTexture(device))
            return false;

        // Create depth buffer
        if (!CreateDepthBuffer(device))
            return false;

        // Create render target views (one per face)
        if (!CreateRenderTargetViews(device))
            return false;

        // Create depth stencil view
        if (!CreateDepthStencilView(device))
            return false;

        // Create shader resource view
        if (!CreateShaderResourceView(device))
            return false;

        // Setup view matrices for cube faces
        SetupViewMatrices();

        // Setup projection matrix (90 degree FOV for cube faces)
        m_projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XM_PIDIV2, 1.0f, 0.1f, 1000.0f);

        return true;
    }

}